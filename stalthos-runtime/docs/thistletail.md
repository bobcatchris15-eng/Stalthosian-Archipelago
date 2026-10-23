# Playable Thistletail and finite microforage

Proposed fictional working draft, October23,2026. Normal Play accepts
`-HeroSpecies Smalltrotter` (default) or `-HeroSpecies Thistletail`. Raw host
`--hero-species` is connected-only and validates the same two choices. Original
saved-mode species restoration remains authoritative and does not accept this
new option. Species does not cycle during life; the ordinary N transition starts
another selected-species life with fresh needs/lifecycle/ID/session.

Thistletail is an actual procedural feather-layered biped hero, separate from the
Smalltrotter GLB retained for NPCs. It has a rounded body, folded layered wings,
feather tail, neck/head/beak/eyes and two legs with splayed toes. Its +Z body axis,
foot datum0 and distance-driven alternating legs use the existing supported root.
The ordinary proxy integrator supplies2m/s direct/autonomous movement; imported
Smalltrotter metadata never sets bird pace. Generic needs priority, drinking,
rest, contextual destination, camera and player override remain their existing
owners. No final anatomical gait, feather simulation or stage-specific art claim.

Only bird connected sessions declare resource210, a finite1unit Shrub-kind food
resource. This reuses existing food semantics without a save enum/schema change.
It is a litter/seed/invertebrate cohort stand-in at24,24, drawn as36grounded litter
and seed shapes. The body can sense it within12m and physically contact it within
0.85m horizontally/0.4m vertically on rendered-supported ground. Remote camera
position is absent from both sensing and contact. The existing HeroDrinkAutonomy
Eat intention owns approach/contact; no second AI/controller or per-insect model.

Eligible contacts use the existing shared registry ledger and InteractionsCore
session idempotence. Each real contact consumes up to0.5 and applies the same
hunger relief. Visible shape count and shadows follow actual remaining fraction.
The resource stays depleted across next life in the running world. Existing NPCs,
identity/session allocation, hatchling admission and full resource history remain
in their existing owners. Default Smalltrotter/original mode receive no210 patch.

Focused verification:

```powershell
cmake --build var/build/nesting-domain --config Release --target Stalthos_thistletail Stalthos_hero_autonomy Stalthos_native_population_registry Stalthos_next_life
ctest --test-dir var/build/nesting-domain -C Release -R '^domain-(thistletail|hero_autonomy|native_population_registry|next-life)$' --output-on-failure
```

After root's final native integrated build:

```powershell
stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -BirdProbe -Duration 35 -AssetRoot 'E:/Stalthosian Archipelago'
stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -BirdOverrideProbe -Duration 24 -AssetRoot 'E:/Stalthosian Archipelago'
stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -BirdCameraProbe -Duration 18 -AssetRoot 'E:/Stalthosian Archipelago'
```

Fixtures explicitly select Thistletail, restore hunger0.8 and startup pose, and
render noon. Primary starts6m from food and follows the ordinary Eat goal; at17
physical seconds only it injects a labelled death state, then the existing N
transition at18. Override injects ordinary reverse1-3s then releases. Camera body
starts36m from food, detaches at2s and pans2-9s without body food knowledge.
Composed captures occur0.5/5/12/20s. `thistletail.json` records actual species,
needs/lifecycle species, physical pose/gait/motion, contact/ledger amount, remaining
food/visible fraction, camera/attention, NPC counts/sessions and successor identity.
Source/domain proof does not establish native visuals or FPS; root owns that gate.

Remaining breadth includes other species, Thistletail childhood/brief parenting,
multiple forage substrates/locations, four islands/physical crossings and connected
saves. This is one bounded contrasting life, not complete five-species coverage.
