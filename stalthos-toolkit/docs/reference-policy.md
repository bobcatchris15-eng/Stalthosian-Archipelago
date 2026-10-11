# Stalthos Toolkit — Reference & Licence Policy

**Version:** 1.0
**Date:** 2026-10-10
**Status:** Binding for all pipeline stages

---

## 1. Purpose

This policy governs how external references (images, taxonomic data, literature, specimen records, planning documents) are sourced, verified, recorded, and attributed in the Stalthos Asset Toolkit. It exists to satisfy the Rauken-Archipelago asset toolkit requirement: *"Research claims require real citations in future completed specifications; don't invent taxonomic evidence."*

**Core principle:** *Honesty over completeness. A small verified registry beats a large unverified one.*

---

## 2. Acceptable Licence Classes

| Licence Class | SPDX ID | Redistributable? | Attribution Required? | Notes |
|---------------|---------|------------------|----------------------|-------|
| **Public Domain / CC0** | `CC0-1.0` | Yes | No (courtesy yes) | Preferred. Includes GBIF, Paleobiology Database, Poly Haven. |
| **CC BY (any version)** | `CC-BY-3.0`, `CC-BY-4.0`, etc. | Yes | **Yes** | Must record attribution string and URL. |
| **CC BY-SA (any version)** | `CC-BY-SA-3.0`, `CC-BY-SA-4.0` | Yes | **Yes** | ShareAlike triggers only if derivative work distributed; our generated assets are not derivatives of reference images. |
| **Own Work** | `own_work` | Yes | No | Created by project contributors. |
| **Permission Granted** | `permission_granted` | Yes | Per agreement | Written permission on file. |
| **Internal Reference** | `internal_reference` | **No** | N/A | Planning docs, internal notes. Never redistributable. |

### Explicitly REJECTED Licence Classes
| Licence | Reason |
|---------|--------|
| CC BY-NC / CC BY-NC-SA / CC BY-ND | Non-commercial or no-derivatives clauses incompatible with asset redistribution. |
| "All rights reserved" / Copyrighted without explicit licence | No redistribution rights. |
| Unknown / Unverified | Treated as `unverified` — recorded but never trusted. |
| PlantCatalog / Bentley-derived material | Explicitly flagged as restricted redistribution per Rauken-Archipelago plan. |

---

## 3. What Must Be Recorded Per Source

Every entry in `research/source-registry.json` **must** contain:

| Field | Required | Description |
|-------|----------|-------------|
| `id` | Yes | Stable slug (e.g., `gbif-araucar-ara`) |
| `url` | Yes | Resolvable HTTP(S) URL or local path |
| `resolved_title` | Yes | Actual page/resource title at time of check |
| `http_status` | Yes | HTTP response code (200, 404, etc.) |
| `license` | Yes | SPDX identifier or `unknown` |
| `license_verification_method` | Yes | How licence was confirmed (API metadata, terms page, header) |
| `date_checked` | Yes | ISO 8601 date (YYYY-MM-DD) |
| `supports` | Yes | Array of definition fields this source backs |
| `confidence_tier` | Yes | `high` / `medium` / `low` |
| `redistributable` | Yes | Boolean — true only if licence permits |
| `kind` | Yes | `taxonomic_database`, `paleontological_database`, `reference_image`, `asset_library`, `planning_doc` |
| `attribution` | If CC BY/SA | Human-readable attribution string |
| `attribution_url` | If CC BY/SA | URL for attribution |
| `notes` | Optional | Caveats, limitations, partial matches |

**Unverifiable sources** go in `research/unverified.json` with:
- `candidate_url`, `intended_use`, `reason`, `date_checked`, `status` (`not_found`, `rate_limited`, `requires_auth`, `local_missing`, `internal_only`)

---

## 4. Attribution in Manifests & Receipts

The `publish_asset` stage (stage 9) writes a provenance manifest at `var/reports/manifests/<asset_id>.json`. The manifest **must** include:

```json
"licensed_references": [
  {
    "source_id": "gbif-araucar-ara",
    "license": "CC0-1.0",
    "attribution": "GBIF.org (CC0)",
    "url": "https://api.gbif.org/v1/species/match?name=Araucaria%20araucana"
  },
  {
    "source_id": "wikimedia-marattia-douglasii-starr",
    "license": "CC BY 3.0 US",
    "attribution": "Forest & Kim Starr, CC BY 3.0 US",
    "attribution_url": "https://www.flickr.com/photos/starr-environmental/24485041734/",
    "url": "https://commons.wikimedia.org/wiki/File:Starr-110929-0072-Marattia_douglasii-habit-Hanaula-Maui_(24485041734).jpg"
  }
]
```

**Rules:**
- **Two different artifacts, two different rules.** This section describes the
  **redistributable credits manifest** — the thing that ships to third parties.
  `publish_asset` writes a **build receipt** to
  `var/reports/receipts/<asset_id>.json`, which is an *internal build record*,
  not a published credits screen.
- In the **redistributable credits manifest**, only sources with
  `redistributable: true` appear, and `internal_reference` sources are excluded.
- In the **build receipt**, all references are copied **verbatim** as
  provenance, including `internal_reference` and unverified entries, alongside
  `unverified_usage_rights` and `usage_rights_block_publication: false`. This
  satisfies intent i-0002's requirement to *record provenance*; a receipt is
  where that record belongs.
- Attribution strings must be copy-paste ready for credits screens.

---

## 5. The Unverifiable Source Rule

> **Any source whose licence cannot be positively verified is recorded in `unverified.json` with the reason. It is NEVER promoted to `source-registry.json`, never marked `redistributable`, and never used to justify a design claim.**

This applies to:
- 404/not found URLs
- Rate-limited API endpoints where metadata couldn't be read
- Sources requiring authentication (Smithsonian Open Access API key)
- Local placeholder files that don't exist
- Internal planning documents (recorded as `internal_reference` in sidecars but excluded from public manifests)

**Rationale:** The project is fiction. Visual reference is the goal, not taxonomic authority. An unverified source adds noise, not signal.

---

## 6. Sidecar Reference Files — Merge Procedure

Each species has a sidecar file at:
- `species/plants/references/<species_id>.yaml`
- `species/creatures/references/<species_id>.yaml`

These files contain a `references:` array matching the schema's `reference` shape, plus `species_id` and `generated_at` metadata.

### Merge Procedure (Worker Zero)
1. Load the species definition YAML.
2. Load the corresponding sidecar reference YAML.
3. **Replace** the definition's `references:` array entirely with the sidecar's `references:` array.
4. Preserve the definition's `licenses:` array (definition-level licenses are separate).
5. Write the updated definition back (or to a staging path for review).
6. Re-run `research_species` to generate a fresh research record.

**Do not** merge by appending — the sidecar is the authoritative replacement for the placeholder references.

---

## 7. Verification Workflow (for Contributors)

When adding a new source:
1. `curl -I <URL>` — confirm HTTP 200.
2. Fetch the page / API response — confirm real title/content.
3. Locate the licence statement (API metadata, page footer, terms page).
4. Map to SPDX ID. If ambiguous → `unknown` → goes to `unverified.json`.
5. Add entry to `source-registry.json` with all required fields.
6. Add corresponding reference entry to the species sidecar YAML.
7. Run `tests/test_references.py` — must pass.

---

## 8. Compliance Checklist (for `publish_asset`)

> **Amended 2026-10-10 under active intent i-0002.** Unverifiable licence
> information is **recorded, not enforced**. `usage_rights: unknown` /
> `unverified` / absent is an allowed, expected state for a non-commercial
> personal project. It produces a warning and an entry in the build receipt's
> `unverified_usage_rights` list. It is **not** a publication blocker. The
> checklist below was originally written to gate on established rights; that
> reading is superseded and an agent following it literally would block assets
> the operator has explicitly permitted.

Before promoting an asset:
- [ ] **Blocking — structural:** validation outcome is `pass`
- [ ] **Blocking — structural:** `status.partial` is false
- [ ] **Blocking — structural:** every artifact exists and its sha256 matches the manifest
- [ ] **Blocking — structural:** provenance records seed, `input_hashes` and `tool_versions`
- [ ] **Blocking — structural:** no promotion-path collision (two artifacts
      flattening onto one target for the same `asset_id`)
- [ ] **Warning only:** any reference carries `usage_rights` outside
      `{cc0, cc-by, cc-by-sa, public_domain, own_work, permission_granted}` →
      warn, record in `unverified_usage_rights`, do **not** block
- [ ] **Warning only:** `source-registry.json` passes `test_references.py`
- [ ] Recording — every redistributable source carries its attribution string

The enforced behaviour lives in `commands/publish_asset.py`. The four structural
blockers are `ProvenanceSeedMissing`, `ProvenanceInputHashesMissing` /
`…Invalid`, `ProvenanceToolVersionsMissing`, `ArtifactMissing` / `ArtifactUnhashed`
/ `ArtifactHashMismatch`, plus `PromotionCollision`. **No licensing condition is
a blocker**; `_licensing_warnings()` appends only to `warnings`.

---

## 9. Change Log

| Version | Date | Author | Change |
|---------|------|--------|--------|
| 1.0 | 2026-10-10 | ReferenceArchivist (W12) | Initial policy |