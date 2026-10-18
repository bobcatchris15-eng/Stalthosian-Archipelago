# Regional habitat candidates and bounded contact admission

Status: Decided draft contract; regional retained population registry remains to implement.

`regional_habitat.hpp` constructs a bounded connected habitat candidate using a
pure caller-owned ground query. A globally aligned lattice belongs to half-open
non-overlapping region bounds. It accepts at most 512 lattice candidates and at
most 100,000 actual ground-query calls. Invalid input, work exhaustion and no
viable habitat return distinct statuses; exhaustion returns no partial habitat.
Only dry, non-wet, finite, slope-eligible samples become sites. Sampled links reject
unsupported ground, water and excessive height changes. A connected component must
contain caller-confirmed browse and dry-bank freshwater access. Seed and region
select among viable components without shared request-order state. Features
narrower than the route-sampling interval can be missed. Cross-region links and a
retained population registry are outside this packet.

The populated Wicked host writes `regional-habitat-candidate.json` for a small
watershed construction probe. Height and slope come from joined rendered terrain
triangles. Browse requires full 3D distance within 0.8m of a supplied shrub. Water
requires a dry bank and an actual water-mask hit at a cardinal 0.25/0.5/0.75m offset,
water-surface height separation at most 0.45m, and 3D reach at most 0.8m. This is a
draft eligibility approximation, not anatomical contact acceptance. It never
replaces the original habitat sites/signature or creates actors. Camera prewarm
therefore cannot create an actual population through this probe.

`ResourceLedger` retains its 256-key hard limit and STL8 DTO. Admission inspection
and nonthrowing mutation report replay, update, new key, full history, missing
resource, invalid request, conflict, insufficient quantity or storage failure.
A full ledger permits existing cumulative contacts and rejects new keys without
quantity deduction or eviction. Capacity exposes used, limit and available slots.
The legacy bool/exception APIs remain compatible.

In the populated host, `Population::advance(dt,false)` advances needs and motion
without local resource relief. The fauna adapter commits contacts to the ledger,
then applies relief only for new or updated consumption. Replays and rejections
emit no fresh event or relief. Accepted food/water rates retain the prior draft
scaling and caps; partially depleted resources grant proportionate relief.
Activity decisions can reflect pre-relief needs until the next frame. Standalone
population callers retain the original default local-relief behavior.

`world-population.json` reports history usage and rejection counts. Rejection counts
are per-process attempted contacts, not persistent unique events. Once full, a new
actor-resource pair can remain denied on every frame; this visible bounded policy
prevents a crash and invented benefits but does not solve long-term capacity.
Production regional population growth still requires explicit admission budgets
and/or a versioned persistent archive design. Never silently evict old contacts.

Validation receipt: `.clanker/tasks/regional-habitat-admission-20261018/receipt.json`.
Existing STL8 worlds require no migration or new world for this change. The
original eight watershed identities and habitat signature are preserved.
