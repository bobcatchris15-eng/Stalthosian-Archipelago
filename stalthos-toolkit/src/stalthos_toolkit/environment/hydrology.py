"""Deterministic hydrology over an existing heightfield.

Display-first scope (see ``Rauken-Archipelago/docs/ecology-and-hydrology.md``):

* Hydrology is a **map property**, computed offline at world generation.
  Nothing here simulates groundwater transport, watershed water budgets,
  sediment advection, erosion or fluid dynamics.
* What we do compute, cheaply and reproducibly, is the *observable*
  relationship the docs demand: ``terrain/substrate -> plausible water and
  exposure -> plant composition -> animal presence``.
* Any quantity we cannot actually derive from the heightfield and the
  hydrology parameters is emitted with ``measured=False`` and a warning.

The pipeline is:

1. :func:`fill_depressions` — priority-flood depression filling (Wipley et al.
   style) with deterministic tie-breaking, so every cell has a downhill or
   sea-level outlet.
2. :func:`route_flow` — steepest-descent single-flow routing on the filled
   surface, then accumulation by processing cells in decreasing filled height.
3. :func:`moisture_suitability` — a map property combining flow accumulation
   (water proximity), relative elevation above the water table, slope-derived
   drainage/shelter, and a seeded per-cell permeability jitter.
4. :func:`summarize` — map-level metrics for the status envelope.

All randomness goes through ``ctx.seed_for`` / ``ctx.rng``; no global
``random``, no ``hash()``.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from collections import deque
from typing import TYPE_CHECKING, Any, Literal

from pydantic import BaseModel, Field

from .heightfield import Heightfield

if TYPE_CHECKING:  # pragma: no cover - typing only
    from ..core.contract import RunContext


# ---------------------------------------------------------------------------
# typed models
# ---------------------------------------------------------------------------


class WaterTableSpec(BaseModel):
    """Parameters of the (offline) water table / moisture model.

    The table is *not* a groundwater solver. ``base_level`` is the standing
    water level the terrain drains toward; ``permeability_jitter`` is a small
    seeded per-cell offset applied when sampling the table, standing in for
    substrate permeability variation.
    """

    base_level: float = Field(
        default=0.0, description="Standing water level, world units."
    )
    gradient: float = Field(
        default=0.0,
        description="Additional rise of the table per unit of relative elevation.",
    )
    permeability_jitter: float = Field(
        default=0.15,
        ge=0.0,
        description="Peak seeded per-cell table offset (substrate permeability stand-in).",
    )
    stream_threshold: int = Field(
        default=8,
        ge=1,
        description="Flow accumulation (in cells) above which a channel is reported.",
    )
    floodplain_cells: int = Field(
        default=0,
        ge=0,
        description="Extra cells beyond a channel that count as its floodplain.",
    )


class FlowRouting(BaseModel):
    """Result of single-flow routing over a filled heightfield."""

    downhill: list[int] = Field(
        description="Index of the receiving cell, -1 for outlets."
    )
    accumulation: list[float] = Field(
        description="Upstream contributing cell count per cell."
    )
    channel_mask: list[bool] = Field(
        description="True where accumulation >= stream_threshold."
    )
    filled_elevation: list[float] = Field(
        description="Depression-filled surface used for routing."
    )
    sea_level: float = Field(
        description="Level below which a cell is treated as open water."
    )
    cell_size: float = Field(description="Horizontal extent of one cell, world units.")
    filled_cells: int = Field(
        default=0, ge=0, description="Cells raised by depression filling."
    )
    max_fill_depth: float = Field(
        default=0.0, ge=0.0, description="Deepest depression filled."
    )
    lake_cells: list[int] = Field(
        default_factory=list, description="Filled cells still below sea level."
    )

    def channel_cell_count(self) -> int:
        return sum(1 for c in self.channel_mask if c)

    def watershed_cell_count(self) -> int:
        """Cells with no receiving cell (outlets)."""
        return sum(1 for d in self.downhill if d < 0)


class MoistureField(BaseModel):
    """Offline moisture-suitability map (0..1), the flora's placement input."""

    suitability: list[float]
    water_table: list[float] = Field(description="Sampled table surface per cell.")
    water_table_metric_measured: bool = Field(
        default=False,
        description="False: the table is a seeded approximation, not a solved field.",
    )
    mean_suitability: float = 0.0
    wet_cell_fraction: float = 0.0
    mean_permeability_offset: float = 0.0


class HydrologyResult(BaseModel):
    routing: FlowRouting
    moisture: MoistureField
    drainage_density: float = 0.0
    dominated_by_rivers: bool = False


class HydrologyWarning(BaseModel):
    code: str
    message: str


# ---------------------------------------------------------------------------
# depression filling
# ---------------------------------------------------------------------------


def _cell_major(width: int, x: int, y: int) -> int:
    return y * width + x


def _neighbours(width: int, height: int, idx: int) -> tuple[int, ...]:
    x = idx % width
    y = idx // width
    out: list[int] = []
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dx == 0 and dy == 0:
                continue
            nx, ny = x + dx, y + dy
            if 0 <= nx < width and 0 <= ny < height:
                out.append(_cell_major(width, nx, ny))
    return tuple(out)


def fill_depressions(
    elevation: list[float],
    width: int,
    height: int,
    sea_level: float,
    *,
    epsilon: float = 1e-4,
) -> tuple[list[float], int, float]:
    """Priority-flood depression filling with deterministic tie-breaking.

    A flat filled plateau is produced by stepping out of depressions by
    ``epsilon``; ties are broken by cell index so the result never depends on
    dict/set ordering. Returns ``(filled, filled_cell_count, max_fill_depth)``.
    """
    import heapq

    n = width * height
    filled = [float(v) for v in elevation]
    closed = [False] * n
    heap: list[tuple[float, int]] = []

    # Seed with the map edge; the border is the implicit outlet.
    for x in range(width):
        for y in range(height):
            if x in (0, width - 1) or y in (0, height - 1):
                idx = _cell_major(width, x, y)
                value = max(filled[idx], sea_level)
                filled[idx] = value
                closed[idx] = True
                heapq.heappush(heap, (value, idx))

    while heap:
        value, idx = heapq.heappop(heap)
        for nb in _neighbours(width, height, idx):
            if closed[nb]:
                continue
            closed[nb] = True
            if filled[nb] <= value:
                # Depression (or exactly at the outlet level): raise to the
                # spill level plus one increment so the fill stays monotone
                # and reproducible.
                filled[nb] = value + epsilon
            heapq.heappush(heap, (filled[nb], nb))

    filled_cells = 0
    max_depth = 0.0
    for idx in range(n):
        delta = filled[idx] - float(elevation[idx])
        if delta > epsilon:
            filled_cells += 1
            if delta > max_depth:
                max_depth = delta
    return filled, filled_cells, max_depth


# ---------------------------------------------------------------------------
# flow routing
# ---------------------------------------------------------------------------


def route_flow(
    heightfield: Heightfield,
    *,
    sea_level: float,
    stream_threshold: int = 8,
    floodplain_cells: int = 0,
) -> FlowRouting:
    """Steepest-descent single-flow routing plus flow accumulation.

    ``floodplain_cells`` widens ``channel_mask`` sideways by that many cells so
    downstream consumers see a floodplain band rather than a one-cell thread.
    """
    width = heightfield.width
    height = heightfield.height
    cell_size = heightfield.cell_size
    filled, filled_cells, max_depth = fill_depressions(
        list(heightfield.elevation), width, height, sea_level
    )

    n = width * height
    downhill = [-1] * n
    for idx in range(n):
        if filled[idx] <= sea_level:
            continue  # open water / already at the outlet level
        best = -1
        best_drop = 0.0
        x, y = idx % width, idx // width
        # Order of neighbour scan is fixed; strict ">" keeps the first winner,
        # so ties never depend on ordering accidents.
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx == 0 and dy == 0:
                    continue
                nx, ny = x + dx, y + dy
                if not (0 <= nx < width and 0 <= ny < height):
                    continue
                nb = _cell_major(width, nx, ny)
                drop = (filled[idx] - filled[nb]) / (
                    cell_size * ((abs(dx) + abs(dy)) ** 0.5)
                )
                if drop > best_drop:
                    best_drop = drop
                    best = nb
        downhill[idx] = best

    # Accumulation: process cells from highest to lowest filled elevation. Cells
    # in filled lakes (no downhill neighbour) accumulate but do not pass flow on.
    order = sorted(range(n), key=lambda i: (-filled[i], i))
    accumulation = [1.0] * n
    for idx in order:
        nb = downhill[idx]
        if nb >= 0:
            accumulation[nb] += accumulation[idx]

    channel_mask = [a >= float(stream_threshold) for a in accumulation]
    if floodplain_cells > 0 and any(channel_mask):
        widened = list(channel_mask)
        queue = deque()
        for idx, is_channel in enumerate(channel_mask):
            if is_channel:
                queue.append((idx, 0))

        while queue:
            idx, dist = queue.popleft()
            if dist >= floodplain_cells:
                continue

            x, y = idx % width, idx // width
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dx == 0 and dy == 0:
                        continue
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < width and 0 <= ny < height:
                        nidx = _cell_major(width, nx, ny)
                        if not widened[nidx]:
                            widened[nidx] = True
                            queue.append((nidx, dist + 1))
        channel_mask = widened

    lakes = [i for i in range(n) if downhill[i] < 0 and filled[i] > sea_level]

    return FlowRouting(
        downhill=downhill,
        accumulation=accumulation,
        channel_mask=channel_mask,
        filled_elevation=filled,
        sea_level=float(sea_level),
        cell_size=float(cell_size),
        filled_cells=filled_cells,
        max_fill_depth=max_depth,
        lake_cells=lakes,
    )


# ---------------------------------------------------------------------------
# water table + moisture suitability
# ---------------------------------------------------------------------------


def water_table(
    heightfield: Heightfield,
    spec: WaterTableSpec,
    ctx: "RunContext",
    *,
    labels: tuple[Any, ...] = ("hydrology", "water-table"),
) -> tuple[list[float], float]:
    """Sampled table surface per cell, plus the mean seeded permeability offset.

    Approximation, deliberately: the table rises gently with relative
    elevation and carries a small seeded per-cell offset standing in for soil
    permeability. It is **not** a solved groundwater field and is always
    reported with ``measured=False``.
    """
    rng = ctx.rng(*labels)
    elevations = heightfield.elevation
    offsets: list[float] = []
    for _ in elevations:
        offsets.append(rng.uniform(-spec.permeability_jitter, spec.permeability_jitter))
    lo = min(elevations)
    hi = max(elevations)
    span = (hi - lo) or 1.0
    table = [
        spec.base_level + spec.gradient * ((e - lo) / span) + offsets[i]
        for i, e in enumerate(elevations)
    ]
    mean_offset = sum(offsets) / len(offsets) if offsets else 0.0
    return table, mean_offset


def moisture_suitability(
    heightfield: Heightfield,
    routing: FlowRouting,
    spec: WaterTableSpec,
    ctx: "RunContext",
) -> MoistureField:
    """Offline moisture-suitability field in 0..1.

    Combines, per the planning docs, *water proximity* (log-scaled flow
    accumulation), *relative elevation above the sampled water table*, and
    *slope-derived drainage* — not merely distance to a river.
    """
    import math

    table, mean_offset = water_table(heightfield, spec, ctx)
    n = len(heightfield.elevation)
    cell_area = routing.cell_size * routing.cell_size

    max_acc = max((max(a, 1.0) for a in routing.accumulation), default=1.0)
    log_max = math.log1p(max_acc)

    suitability: list[float] = []
    for idx in range(n):
        elevation = heightfield.elevation[idx]
        slope = (
            heightfield.slope_degrees[idx]
            if idx < len(heightfield.slope_degrees)
            else 0.0
        )

        # Water proximity: saturating in accumulated upstream area.
        proximity = math.log1p(routing.accumulation[idx]) / (log_max or 1.0)

        # Elevation above the sampled table: shallow water table -> wetter.
        relief = max(elevation - table[idx], 0.0)
        shallow = 1.0 / (1.0 + relief / max(cell_area, 1e-6))

        # Slope drains: steep ground sheds water (exposed ridges), flats hold it.
        flat = max(0.0, 1.0 - slope / 45.0)

        value = 0.45 * proximity + 0.35 * shallow + 0.20 * flat
        # Standing water is the wettest thing there is.
        if routing.accumulation[idx] <= 1.0 and elevation <= routing.sea_level:
            value = 1.0
        suitability.append(min(1.0, max(0.0, value)))

    mean = sum(suitability) / n if n else 0.0
    wet_fraction = sum(1 for s in suitability if s >= 0.6) / n if n else 0.0
    return MoistureField(
        suitability=suitability,
        water_table=table,
        water_table_metric_measured=False,
        mean_suitability=mean,
        wet_cell_fraction=wet_fraction,
        mean_permeability_offset=mean_offset,
    )


def summarize(
    heightfield: Heightfield,
    result: HydrologyResult,
) -> dict[str, Any]:
    """Map-level hydrology metrics for the status envelope.

    ``drainage_density`` and ``dominated_by_rivers`` are genuine derived
    quantities. Discharge is **not** reported at all here rather than invented;
    the caller emits ``Metric(measured=False)`` for anything hydraulic.
    """
    n = len(heightfield.elevation)
    if not n:
        return {
            "cells": 0,
            "drainage_density": 0.0,
            "dominated_by_rivers": False,
            "channel_cell_fraction": 0.0,
            "lake_cells": 0,
            "max_accumulation": 0.0,
        }
    channels = result.routing.channel_cell_count()
    return {
        "cells": n,
        "drainage_density": channels / float(n),
        "dominated_by_rivers": result.dominated_by_rivers,
        "channel_cell_fraction": channels / float(n),
        "channel_cells": channels,
        "lake_cells": len(result.routing.lake_cells),
        "max_accumulation": max(result.routing.accumulation),
        "depression_filled_cells": result.routing.filled_cells,
        "max_depression_fill": result.routing.max_fill_depth,
        "mean_moisture_suitability": result.moisture.mean_suitability,
        "wet_cell_fraction": result.moisture.wet_cell_fraction,
    }


# ---------------------------------------------------------------------------
# orchestration
# ---------------------------------------------------------------------------


def analyze(
    heightfield: Heightfield,
    *,
    sea_level: float,
    spec: WaterTableSpec | None = None,
    ctx: "RunContext",
) -> HydrologyResult:
    """Full offline hydrology pass: fill -> route -> moisture."""
    spec = spec or WaterTableSpec()
    routing = route_flow(
        heightfield,
        sea_level=sea_level,
        stream_threshold=spec.stream_threshold,
        floodplain_cells=spec.floodplain_cells,
    )
    moisture = moisture_suitability(heightfield, routing, spec, ctx)
    n = len(heightfield.elevation)
    channels = routing.channel_cell_count()
    return HydrologyResult(
        routing=routing,
        moisture=moisture,
        drainage_density=(channels / float(n)) if n else 0.0,
        dominated_by_rivers=(channels / float(n) >= 0.2) if n else False,
    )


# Alias kept descriptive for callers that think in terms of a map stage.
HydrologyStage = Literal["hydrology"]
