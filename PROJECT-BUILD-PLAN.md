# The Stalthosian Archipelago — full draft build plan

**Date:** 2026-10-11. **Status:** Proposed execution plan for a **full first draft of the actual game**, grounded in the existing planning documents and a current toolkit audit. The delivery target is an integrated draft covering the whole designed experience. Reference scenes and incremental integrations validate parts of that draft as construction proceeds.

## Full draft delivery contract

The first major deliverable is a playable, integrated draft with all planned domains represented and the complete experience connected end to end:

- All five Hero Beasts have their distinct controls, anatomy, ordinary autonomous behavior, childhood, growth, reproduction/parenting, aging and death.
- All four main islands have traversable draft geography, coherent habitat composition, water and environmental hazards, and physical travel between islands where an animal is capable of it.
- HeroView includes orbit/follow, bounded detachment, documentary framing, contextual intentions, alarm recall and independent camera audio. HeroSim retains its own senses and one intention pipeline.
- Flora, wildlife, weather, visible interactions, regional activation and persistence work together. Saves and successive lives preserve conspicuous changes and eligible relationships.
- The toolkit supports producing, inspecting, importing and rebuilding the draft's content, with explicit failures and provenance.

**Draft quality means complete functional coverage with uneven refinement.** Some models, animations, habitat layouts, sounds, materials and tuning may be provisional, clearly identified and scheduled for revision. A proxy is acceptable for an unfinished asset only when it can exercise the actual required function and body plan; it cannot stand in for a missing lifecycle, parenting model, camera behavior or island. Catalog expansion remains progressive because the planning documents treat ~200 plant forms and ~50 non-hero actors as flexible breadth ambitions.

Track every designed capability as implemented in draft, provisional, blocked or missing. Full-draft completion requires all planned capability areas to be connected and usable, with remaining refinement documented. A single polished animal or reference valley is an intermediate validation result. Final visual polish and release acceptance follow the full draft.

## 1. What we are building

A Windows dinosaur life simulator in a persistent, fictional Late Cretaceous-inspired archipelago. A player hatches into an existing population, lives as an animal, grows, optionally reproduces and cares for young, ages and dies. Subsequent lives inhabit the same continuing world.

The distinguishing experience is convincing ordinary life: beautiful terrain and vegetation, believable bodies and movement, quiet wildlife activity, environmental hazards, and recognizable places and relationships. Direct steering expresses intention; compatible head, mouth, gaze and social activity continue. When uncommanded, the animal lives autonomously. The documentary camera observes that life.

**Decided from the design:** five Hero Beasts; four main islands; shared world time; separate camera and animal perception; shallow remote ecology; bounded persistence of conspicuous changes and meaningful individuals; display fidelity before internal fidelity. No quests, crafting progression, inherited character bonuses, omniscient predators, compulsory tourism or closed ecological accounting.

**To test:** native 1920×1080, GTX 1080 8 GB and first-generation Ryzen, 30 FPS floor. No runtime performance acceptance has been established by this audit.

## 2. Authority and scope

- `Rauken-Archipelago/docs/` remains read-only design input. Implementation plans and code belong outside that clone.
- The current user request authorizes analysis and planning for the actual game. Older statements that gameplay was not yet requested describe the earlier planning phase; they do not prohibit planning the game now.
- Latest decisions in `README.md`, `vision-and-principles.md` and the dated decisions in `research-register.md` resolve older heavyweight simulation proposals. Preserve history without implementing superseded requirements.
- `stalthos-toolkit/docs/toolkit-architecture.md` remains the asset-tool contract. Do not rewrite its frozen API during planning.
- Existing dirty terrain/water/schema/optimization work is preserved. Review and finish it with its ownership/state understood before integrating or replacing it.
- All new architecture, directory names, milestone gates and task IDs below are **Proposed**, unless explicitly identified as established design.

## 3. Document analysis and implementation map

| Planning document | Meaning for implementation | First concrete deliverable |
|---|---|---|
| `README.md` | Domain index, shallow simulation ceiling, catalog breadth and precedence | Traceable requirements and decision register |
| `vision-and-principles.md` | Ordinary animal life, visual quality, restrained recurring work | Representative image/motion targets and frame workload |
| `gameplay.md` | Entire hatch-to-death loop; readable condition; optional reproduction | Complete lifecycle scenario matrix, then playable milestones |
| `geography.md` | Shared drowned uplands; distinct four-island characters; provisional dimensions | Composed reference watershed and archipelago blockout |
| `world-generation.md` | Offline terrain/habitat toolbox; local 3D cliffs/caves; animal-scale review | Terrain bundle with collision, masks, routes, landmarks and provenance |
| `simulation-architecture.md` | Nine named in-process domains and singular state authority | Small domain contracts and engine adapter boundary |
| `stalthos-sim.md` | Coarse weather/season/wetness and preset floods | Cached environment state and water-safety changes |
| `flora-sim.md` | Seeded patches, visible browsing, saved tree exceptions | Browse/depletion/recovery and persistent fallen tree |
| `fauna-sim.md` | Coarse populations, staged local actors, bounded retained identities | Rest/forage/react actors independent of camera visibility |
| `hero-sim.md` | One intention pipeline; body-based senses; concurrent activities | Movement/arbitration, short attention and contact behavior |
| `hero-view.md` | Independent camera/control axes; tether; audio; contextual chooser | Camera and input test course including ridge reveal |
| `spatial-activation.md` | Prewarm entire reachable camera envelope; hysteresis; bounded queues | Budgeted region preparation and repeat-visit diagnostics |
| `ecology-and-hydrology.md` | Habitat → water/exposure → plants → animals; shallow internals | Shared tags and finite visible interactions |
| `flora.md` | Structural botanical families and six pipeline reference plants | Family-specific masters and interaction metadata |
| `creatures.md` | Five distinct anatomies, childhoods and social/reproductive lives | Hero anatomy/rig/behavior briefs and stage variants |
| `fauna-actor-catalog.md` | Expandable ~50 non-hero performances, beyond the five heroes | Reviewed actor assets, routines, calls and placement profiles |
| `plant-visual-catalog.md` | Expandable ~200 distinguishable forms, not recolors | Silhouette/leaf atlases and habitat diversity review |
| `visual-direction.md` | Contact, light, grounded materials, motion, LOD coherence | Neutral/outdoor/wet renders and animal-height scene walks |
| `rendering-experiments.md` | Controlled candidates, full-scene frame-time evidence | Common scene/route manifests and captured release builds |
| `asset-toolkit.md` | Reproducible offline workshop, standard outputs, truthful failures | Real pipeline acceptance and runtime import |
| `research-register.md` | Accepted decisions, historical supersession, verified research discipline | Open decisions with evidence and downstream consequences |

### Design tensions that need explicit handling

1. **Engine selected — Decided, 2026-10-11:** the user selected Wicked Engine. Use its MIT-licensed native C++17 runtime with DX12, pinned source and reproducible builds. Renderer comparison and a Flax licensing exception are no longer on the critical path. Performance and visual acceptance still require representative target-hardware evidence.
2. **Geology toolbox versus composition:** the ten-stage generator proposal is optional. Start with coherent drainage and composed relief; do not build ancient-earth simulation to obtain a good ravine.
3. **Food/population conservation:** a remaining FaunaSim acceptance line mentions cohort debits/credits. The later display-first decisions supersede exact accounting. Keep real follower movement and known-ID exclusion; do not infer a mandatory population ledger.
4. **Tree stability:** near revisits retain placements/activity through a grace window. Long dormancy can refresh unimportant scenery; landmarks, observed damage and retained individuals stay coherent.
5. **Time:** world/calendar progression and responsive physical motion need explicit semantics. Contextual slow-time, resting and nesting must not double-charge actions or make control unresponsive.
6. **Visual target:** user-selected reference imagery is still open. Early scenes can evaluate composition and quality, but cannot claim final artistic acceptance before a reference set is agreed.
7. **Numbers:** island dimensions, camera radii, activation/update budgets, mass, lifespan and growth rates remain tunable. Catalog counts express creative breadth rather than mandatory release totals.

## 4. Current implementation and toolkit readiness

Audit baseline: root Git HEAD `abe3b3f` with pre-existing modified/untracked toolkit work. This document does not establish a clean baseline or integration approval.

| Item | Current evidence | Consequence |
|---|---|---|
| Installed package | `stalthos --version` → `stalthos 0.1.0`; Python 3.14.5 | Reuse installed toolkit, verify environment reproducibility |
| Command registry | Twelve planned stage handlers registered | Handler presence is not full body-plan or visual readiness |
| Blender | Doctor finds Blender 5.2; real Softbough generation reports 5.2.0 LTS | Headless generation works for the tested shrub |
| FFmpeg / Git / LFS | Doctor finds FFmpeg 9.0.2 and Git; LFS reports 3.7.1 | Review/media and source versioning tools partly available |
| Optimization | gltfpack and meshoptimizer not discoverable | Real optimizer path must be installed/configured and exercised |
| Compression | Compressonator and KTX not discoverable | Select actual runtime formats, then complete required compression path |
| Validation | glTF validator not discoverable; glTF Transform path exists | Test executable invocation and exported asset validation |
| Profiling | PresentMon and RenderDoc not discoverable by toolkit | Check other installed locations before installing; build capture harness |
| Image assembly | ImageMagick not discoverable; FFmpeg available | Verify real montage/reel output rather than assuming missing deliverable |
| Profiles | 241 plant and 54 creature YAML files | Definitions are not 295 visually accepted assets |
| Creature coverage | `build_creature` supports quadrupedal/cursorial-quadruped proxies, other postures exit 5 | Biped, serpentine, flying and other families need production support |
| LOD completeness | `generate_lods` documents an unimplemented impostor bake and a decimated-shell collision proxy | Add real distant representations and suitable collision only when needed |
| Frame-time benchmark | `benchmark/measure.py` has no running-scene harness; returns unmeasured frame time | Offline file metrics cannot pass the game performance gate |
| Terrain/water | Heightfield, hydrology, biomes and optics import on this machine; terrain/water commands absent from twelve-command CLI | Import success is not integrated generation or visual acceptance |
| Documentation | README says all handlers are unimplemented; runbook/handoff contain older availability and status claims | Repair operational docs from verified current behavior |
| Test suite | `PYTHONPATH=src python -m pytest -q`: **1908 passed, 2 skipped in 267.51s**, exit 0 | Current working-tree suite passes; this does not establish production visuals or all real-tool paths |

**Real command evidence:** `research_species` for `species/plants/softbough.yaml`, seed 42, exited 0, produced a hashed report, and warned about three unresolved locators (two planning paths and one photo). It normalizes offline provenance; it did not perform new research. `generate_plant` then exited 0 through real Blender, producing a `.blend`, result JSON, manifest and log. Its status reported 2,592 triangles and 1,584 vertices. Those are generator outputs, not evaluated runtime GPU costs or visual approval.

Outputs are isolated in `stalthos-toolkit/var/planning-audit-20261011/`. This audit did not publish assets into a game or change existing source code.

See `stalthos-toolkit/docs/project-readiness-2026-10-11.md` for audit commands, scope and concrete gaps. Planning-input hashes and doctor/list stdout are preserved under the audit workspace's `reports/readiness/`.

### Toolkit correction queue

| ID | Priority | Work | Acceptance |
|---|---|---|---|
| TK-01 | First | Pin Python/package/tool paths and versions; check declared dependencies against imported dependencies, including Pydantic in unfinished environment modules | Clean reproducible environment can run documented commands |
| TK-02 | First | Fix provenance path resolution; distinguish absent photo from wrongly rooted planning-document locator | Existing local docs resolve; absent inputs remain truthful warnings |
| TK-03 | First | Reconcile README/runbook, installed console entrypoint and PowerShell runner with actual source | One documented invocation works outside accidental cwd assumptions |
| TK-04 | First | Inventory real tool locations; configure/pin optimizer, compressor and validator needed by chosen interchange/runtime | Real binaries process fixtures and production sample files |
| TK-05 | First | Run six reference plants plus Smalltrotter through actual generation/bake/variants/rig/LOD/review/validation/publish in an isolated workspace | Hashed stage receipts, visible outputs and all degradation disclosed |
| TK-06 | First | Repeat sample builds; compare artifact bytes, canonical geometry and manifest semantic hashes separately | Determinism matches stated contract; time/log differences are not hidden by a semantic hash |
| TK-07 | Next | Review existing terrain/water work; finish schema validation, integration and tests without replacing salvage wholesale | Real reproducible terrain/water bundle, readable masks and rendered seam checks |
| TK-08 | Next | Extend true morphology/rig/material/animation families, starting with a contrasting biped and plant families | Outputs satisfy silhouettes, proportions, stage behavior and contact metadata |
| TK-09 | Next | Complete needed impostor/animation LOD and collision representation | Motion/LOD comparison and physical passage tests pass |
| TK-10 | Next | Add shared scene/camera route manifest and runtime capture/import adapters | Scene costs measured in running packaged build; no offline FPS inference |
| TK-11 | Ongoing | Improve source-backed profile depth and credits/provenance quality | Verified claims and fictional features separated; coverage never fabricated |

Unknown/unverified usage rights remain warning-and-record conditions under existing human intent. Structural provenance gaps, missing artifacts, failed validation and hash mismatches remain blockers. This plan does not impose a new blanket publication approval flow.

## 5. Runtime architecture to implement

**Proposed:** retain the nine named domains as small in-process modules, with engine-specific presentation/physics/navigation/audio integration. Python remains offline asset tooling; native C++17 implements the runtime domains and Wicked adapters.

| Domain | Full draft responsibility |
|---|---|
| SimCore | One calendar clock; explicit time scaling; deadline/event scheduling; bounded activation queue |
| StalthosState | Versioned saves; seeded regional baseline; compact exceptions; hero, nests and bounded retained identities |
| StalthosSim | Coarse weather/season; local modifiers; salinity/water safety; predefined flood levels/masks |
| FloraSim | Spatial batches; patch forage and recovery; stable landmark/tree exceptions; localized appearance changes |
| FaunaSim | Habitat pools/densities; local staging; cheap activity families; active/retained exclusions and recent occupancy |
| BeastCore | Shared senses/reach, species locomotor capabilities, simple condition, age stages and behavior primitives |
| HeroSim | One intention arbiter; direct/destination/autonomous priorities; short expiring attention; compatible body activities |
| InteractionsCore | Contact/reach validation; one application of visible transfers; local outcome events |
| HeroView | Body-relative input; separate camera focus; tether/documentary/alarm framing; invoked chooser; camera audio listener |

Contracts need only IDs, coordinate/unit conventions, seeded profile data, world timestamps, compact region snapshots, intent source/status, body/perception output, local interaction events and changed presentation records. Avoid a generic universal event/importance framework.

**Construction layout:** retain `stalthos-toolkit/`; introduce `stalthos-runtime/` for the game and engine adapters, `project-docs/` for implementation decisions and `tests/scenarios/` for shared scenario inputs. Root `var/` or explicitly configured workspace paths hold derived assets/captures. Decide heavyweight source storage and LFS patterns deliberately. Do not put runtime code in the planning clone.

## 6. Construction sequence for the full draft

M0–M5 are cumulative construction and validation steps toward one integrated full draft. They do not define separate reduced product scopes. Keep all five species, four islands and complete lifecycle requirements visible in the backlog from the outset; integrate their draft representations as dependencies become available.

### M0 — Establish a trustworthy production baseline

Complete TK-01 through TK-06. Capture exact hardware/driver, build tools, free space, versions, dirty-work ownership and reproducible invocation. Inventory engines already on disk before downloading/building anything.

**Exit:** meaningful suite passes; reference asset chains run with real tools; missing/degraded steps are explicit; representative rendered results have been inspected. No engine or game-performance claim follows from this gate alone.

### M1 — Shared representative scene and renderer decision

Create an engine-neutral ~512×512 m benchmark scene and a separate proposed ~1×1 km traversable geography patch. Use the same terrain, vegetation transforms/material inputs, water, animated animal, weather and camera paths in the pinned Wicked runtime. Start with Main Island creek/ravine/woodland/outcrop/open ground, then include a karst/overhang fixture to test 3D terrain needs.

Compare geometry → foliage → wind → shadows → animals → weather, then the complete workload. Include juvenile-height traversal, sudden turn, ridge reveal, chunk crossing, water/rock edge, gathering and storm routes. A visually sparse control is diagnostic only.

**Exit:** standalone release build(s), exact scene/hash/settings, cold and warm routes, at least three steady trials, raw frame-time traces and corresponding still/moving images. Report median/P95/P99, 1% lows, frames over 33.33 ms, hitches, CPU/GPU evidence and memory. Proposed experiment gate follows the design's P99 ≤33.3 ms with no recurring severe hitches; do not relabel it as a guarantee every game frame meets the floor. Diagnose specific rendering costs and revise the Wicked configuration or implementation when the gate fails.

### M2 — Embodiment, interaction and regional continuity

Implement the named domain contracts for the full draft, then exercise them first with a Smalltrotter controller with actual terrain contact; body-relative W at all camera headings; broadside translation; direct steering's projected central-third detour budget; obstruction inspection; TravelTo; compatible walking/browsing; autonomous rest/forage/drink. Integrate tethered/detached camera and two independent hearing perspectives. Keep the interfaces species-neutral and include contrasting body-plan fixtures before treating the controller as shared.

Add seeded flora batches, staged resting/foraging wildlife, prewarm beyond the entire camera envelope, grace/hysteresis and version-safe async results. Add browsed patch, fallen tree and witnessed death persistence early, with save/load and region cycling.

**Exit:** the animal lives when uncommanded; camera-only knowledge stays out of animal perception; ridge reveal does not instantiate a herd; returning preserves recognizable changes; no duplicate contacts/individuals. Capture these in motion and measure activation spikes.

### M3 — Complete lifecycle foundation, exercised through Smalltrotter

Build Smalltrotter's hatching, independent early life, growth through adulthood, courtship/nesting/hatching, aging and death. Shared world time supports interruptible rest/nesting. Implement successor hatching in the continuing population/world, persistent nests/carcasses and restrained qualitative condition feedback. Upgrade the anatomical proxy into reviewed production anatomy, stage proportions, materials, gait, neck/reach, bristles and armor.

**Exit:** a reproducible complete life scenario and successor life work in the persistent world. Ordinary voluntary commands remain strongest; food/water behavior respects reach and salinity; growth visibly changes clearance/reach/motion. Lifecycle and pacing remain in scope even while content is incomplete.

### M4 — All five playable lives and family behaviors

Add heroes in reusable-family order, subject to visual acceptance: Brackenback, Reedcrest, Thistletail and Longstride. Exercise both quadrupedal and bipedal locomotion rather than treating every animal as Smalltrotter.

- Brackenback: communal young, low browsing, suspicious investigation and situational protection of unrelated distressed small animals.
- Reedcrest: synchronized hatch cohort and reciprocal bonded peers; members can lead, separate, search, survive or die without teleporting.
- Thistletail: feathered biped, real microforage, brief intensive parenting and dispersal.
- Longstride: motivated episodic hunting/scavenging, wild caregiver for playable hatchling, and prolonged dependent young following a player parent.

**Exit:** each species supports hatching-to-death, reproduction and its distinct relationships. Wild and playable conspecifics use compatible shared rules. Save/load, lost companions, cross-region and failed crossings preserve identity/death truth.

### M5 — Complete integrated full draft: archipelago, habitats and cast

Block out four islands together against shared ridge/drainage/coastal logic. Populate in habitat packages, expanding from accepted generators and shared animation/activity families. Main Island establishes rocky woodland/sandy lowland/ria transitions; Rainridge wet forests and gorges; Stone Island vegetated karst/cenotes; Outer Reach exposed mineral plateau/pinnacles and green hollows.

Produce plant silhouette/leaf atlases and fauna silhouette/motion reels before counting forms as accepted. Introduce weather, seasons, temporary flood hazards, shore landings and real species-appropriate crossings. Expand toward the ~200 plant/~50 non-hero palette progressively; never use YAML count or ecological equilibrium as acceptance.

**Exit:** all four islands, all five complete playable lives, camera/control modes, environment, flora/fauna, local interactions, saves and successive-life continuity operate together in a packaged draft. Islands have distinct recognizable character and locally coherent transitions; ordinary quiet life and travel remain satisfying; content additions primarily require assets/profiles/tags. Final sizes follow traversal testing. Review the full draft delivery contract and scenario register for missing coverage; record provisional assets and refinement work explicitly.

### M6 — Revise the full draft, polish and establish release readiness

Test mature dense scenes, mixed wildlife, all hero stages, storms, underwater/wading transitions as applicable, fast reveals, cold streaming, long-absence visits, repeated deaths, long parenting, save migration and sustained sessions. Review animation contact, foliage bending, water edges, audio perspective, condition legibility, accessibility, remappable controls and recoverable save behavior.

**Exit:** reproducible Windows package, measured full-scene performance on reference hardware, approved motion/image quality, stable saves across lives, bounded preparation/save size, unattended asset rebuilds and known limitations stated. The final performance objective remains stricter than a favorable average or one early benchmark.

## 7. Dependencies and work sequencing

Critical path: **toolkit truth → common scene/import/capture → eligible engine decision → shared embodiment/staging/persistence → complete lifecycle foundation → five life histories and four-island content → integrated full draft → revision and final acceptance**.

Reference research, source-art improvements, shared profile/tag contracts and terrain composition can advance while engine tests are prepared. Wicked-specific shaders/importers and streaming contracts can now proceed in dependency order. Delay bulk catalog generation until contrasting family outputs pass visual review.

The first useful bounded work packets are:

1. **BUILD-01:** capture toolkit/environment baseline and close invocation/provenance defects (TK-01–03).
2. **BUILD-02:** real optimization/compression/validation and reference asset-chain acceptance (TK-04–06).
3. **BUILD-03:** finish/review existing terrain, hydrology, biome and water work; export a composed reference patch (TK-07).
4. **BUILD-04:** common scene/route manifest, import adapter and native capture harness (TK-10/M1).
5. **BUILD-05:** Wicked readiness evidence report and required corrections (M1).
6. **BUILD-06:** implement/test embodiment, continuity and perception scenarios (M2).

These are proposed packets, not dispatched workers or claims of implementation. Estimate duration only after the first real chains and candidate scenes reveal throughput and failure costs.

## 8. Scenario and evidence register

| Scenario | Required evidence |
|---|---|
| W with front/broadside camera | Recorded input, body motion and camera framing; no camera-relative steering |
| Small obstacle versus major detour | Local correction versus stop/inspect; TravelTo can use wider feasible route |
| Walk and browse | Feet continue, head reaches foliage, visible depletion commits once |
| Detached ridge/audio observation | Staged wildlife; player sees/hears what animal does not; no sensing leak |
| Autonomous documentary observation | Continuous ordinary animal life; smooth camera; intervention remains immediate |
| Alarm recall/contextual chooser | Only animal-sensed alarm recalls; camera input cancels; chooser appears on request |
| Nearby revisit and long dormancy | Stable near placements, persistent tree damage/death, bounded reconstruction |
| Peer/parent/dependent continuity | No duplicate/resurrected animal; real separation and passage failures |
| Accelerated nesting/rest | Shared calendar; responsive input/reflexes; threats interrupt; compatible NPC stages |
| Hero death and successor | Continuing environment, nests and eligible identities; ordinary new hatchling |
| Dense full scene and cold streaming | Raw release-build frame-time trace, visuals, settings, scene hash, CPU/GPU/memory evidence |

Store receipts by gate/run ID, with source revision plus dirty input hashes, tool/engine versions, definitions and asset hashes, camera/input route, settings, command/exits, raw logs/captures, images/video and pass/fail reasons. Label **diagnostic**, **feasibility** and **acceptance** separately. Keep failures.

## 9. Decisions still open

- Smalltrotter primary user image is Decided: `stalthos-toolkit/references/creatures/smalltrotter/user-primary.png`. Reference anatomy, materials and motion review remain To test; remaining species and environment image targets are still open.
- Refine the selected Wicked/C++17 build/import workflow using M1 evidence.
- Actual reference CPU/RAM/storage/driver, engine availability and build toolchain locations.
- World scale, developmental and seasonal pacing, camera tether, activation budgets and retained-identity limits, resolved by representative tests.
- Release content breadth and production time, resolved after accepted family throughput is known.

**Recommended immediate action:** close toolkit invocation/provenance/tool gaps and prove a reference asset chain, then turn those same assets into the common terrain/foliage/animal benchmark. Use that evidence to start construction of the integrated full draft, keeping complete system, species, geography and lifecycle coverage as the delivery target throughout.
