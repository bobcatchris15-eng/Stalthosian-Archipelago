# Smalltrotter Bristle Form

## Geometry Profile and Dimensions
The bristle ridge geometry has been refined to move away from heavy, thick spikes to fine, irregular swept fibers that visually match the reference anatomy (`user-primary.png`).

- **Root Width**: Max radius of `0.04 * length`. (Previously `0.22 * length`).
- **Mid Width**: Radius of `0.02 * length`.
- **Tip Width**: Radius of `0.005 * length`.
- **Sweep and Breakup**: Bend limits and jitter have been expanded using deterministic random limits (`rng.uniform(-0.25, 0.25)` and higher sweep variations) to create loose, backward-curved natural clumps instead of an evenly stepped comb.

## Mesh cost and evidence boundary

Each fiber keeps three five-sided rings and two capped ends. Two tube spans produce 20 side triangles; triangulated caps add further triangles. Exact exported counts will be measured from the generated GLB. The previous claim of ten triangles per fiber omitted the second span and caps and is rejected.

The radius profile is proposed at 0.04, 0.02 and 0.005 times sampled length. Anchor paths and the caller-provided seeded RNG remain unchanged. A fresh Blender build/rig/render, artifact hashes and native review are required before promotion. This changes geometry only; bristle shading and rig weighting remain separate work. No artistic-reference parity is claimed.
