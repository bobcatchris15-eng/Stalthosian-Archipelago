"""Stage 8 measurement harness.

`measure.py` reads what can actually be read off disk (geometry counts, byte
sizes, buffer-view footprints) and refuses to invent what cannot (frame time,
driver VRAM, streaming wall-clock). Everything it reports carries a `measured`
flag.
"""

from __future__ import annotations

from .measure import (
    GTX_1080_BUDGET,
    BenchmarkReport,
    Measurement,
    ReferenceBudget,
    budget_context,
    compare_before_after,
    frame_time_measurement,
    measure_glb,
    streaming_cost,
    vram_estimate,
)

__all__ = [
    "GTX_1080_BUDGET",
    "BenchmarkReport",
    "Measurement",
    "ReferenceBudget",
    "budget_context",
    "compare_before_after",
    "frame_time_measurement",
    "measure_glb",
    "streaming_cost",
    "vram_estimate",
]