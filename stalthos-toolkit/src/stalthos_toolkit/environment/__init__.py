"""World environment synthesis for The Stalthosian Archipelago.

Blender-independent, network-free, pure-stdlib terrain / water / biome
generation. Composed of:

* :mod:`heightfield` -- deterministic heightfield synthesis (seeded value
  noise + continental mask + ridged relief + domain warp).
* :mod:`hydrology` -- presentation-grade drainage: priority-flood fill,
  D8 flow accumulation, channel networks, lakes/wetland basins.
* :mod:`biomes` -- elevation/slope/aspect/moisture/substrate/temperature ->
  habitat classes matching the species ``habitat_tags`` vocabulary.
* :mod:`terrain` -- the orchestrator producing a terrain descriptor + the
  deterministic per-chunk manifest for the spatial-activation model.

Modules are imported lazily by the command so importing this package stays
cheap; see ``docs/environment-generation.md`` for the design and its explicit
approximations.
"""

__all__ = ["heightfield", "hydrology", "biomes", "terrain"]
