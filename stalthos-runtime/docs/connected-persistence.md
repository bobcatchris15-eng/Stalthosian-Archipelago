# Partial connected persistence draft

STLC version 1 is a separate, explicit manual connected profile, not a relaxed STL9
reader. Its payload embeds the existing host/registry transaction with a distinct
configuration fingerprint plus exact next/current physical interaction sessions,
calendar scale and body wetness. The embedded host retains needs, lifecycle,
clocks, body/camera, tree exceptions, prior deaths and full authoritative population,
resource ledger and bindings. The next contact after reload allocates a fresh session;
held/in-flight contact is refused before saving, so the transient InteractionsCore
cache cannot replay relief from a prior key.

From profile entry social learning, courtship, automatic exploration/foraging and
protective hero goals are unavailable and are advertised before launch and in HUD.
Ordinary body movement, manual E contact, R rest, N after death, camera/destinations
and the real single-owner NPC simulation remain. No live ordinary session is
converted, cleared or silently downgraded. Saves refuse outstanding direct/destination
control, movement, held contact, pending travel/recall, custom tethers, nest/mate/bond
or dynamic-habitat state. No native corpse/hatching/bond persistence claim.

Independent admission requires the generated baseline roster IDs/species (including
records now dead), selected resource set/kinds/authored initial quantities and
unchanged habitat/anchors. A stable fingerprint includes exact imported asset bytes
and clip duration/stride/sole configuration. Seed mismatch and terrain body-datum
mismatch refuse. The schema/configuration revision must be bumped when native
immutable layout/construction semantics change. Existing original STL8/9 policy
and original connected --save-state/--load-state refusal are unchanged.

The file is bounded, checksummed, candidate-decoded and reconstructed before atomic
publication; failures clean only their own temporary candidate. Existing destinations
must themselves be valid matching STLC files. A reconstructed live owner is recaptured
before its first tick and exact canonical bytes must equal the loaded candidate.
Evidence records those actual recaptured bytes rather than echoing the decoded file.

Focused evidence: domain-connected_save, domain-native_save and domain-weather_response
pass. Native host compile/process-restart/gameplay evidence is root-owned and pending.

Native diagnostic commands (only task-owned ignored save paths):

```powershell
pwsh -File stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -HeroSpecies Longstride -ConnectedPersistenceProbe -ConnectedSave 'E:/Stalthosian Archipelago/var/test-saves/connected-persistence-20261023/first.stlc' -Duration 22 -AssetRoot 'E:/Stalthosian Archipelago'
pwsh -File stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ConnectedLoad 'E:/Stalthosian Archipelago/var/test-saves/connected-persistence-20261023/first.stlc' -ConnectedSave 'E:/Stalthosian Archipelago/var/test-saves/connected-persistence-20261023/restarted.stlc' -Duration 8 -AssetRoot 'E:/Stalthosian Archipelago'
```

The labelled probe uses supported authored carrion body startup, synthetic hunger,
ordinary contact input at physical seconds 2 and 9.2, repeat hunger at9, actual
forward/reverse movement10..11, synthetic death12 and ordinary N13. It never moves
or duplicates NPCs or injects resource depletion. It requests the same F5 checkpoint
at5 and saves again on exit. Compare final connected-saved-candidate.json to the
restart connected-restored-before-first-tick.json simulation_bytes_hex, allocator,
physical session, wetness and scale exactly. The runtime refuses if actual rebuilt
owner differs. Reports label failures; state_saved is true only after a real write.

The same F5 path is requested during actual injected movement at10.2s. Its event records refusal, exact destination-byte equality and exact current-world candidate equality across the call; simulation continues to N and the final save.
