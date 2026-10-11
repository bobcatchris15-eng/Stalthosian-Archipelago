"""Deterministic heightfield synthesis for the Stalthosian Archipelago.

Pure Python standard library. **No** ``random``, **no** builtin ``hash()``, and
**no** NumPy are used here: every value is a pure function of an integer seed
and the world-space cell coordinate, so the same ``(definition, seed)`` always
produces a byte-identical float grid, and any rectangular sub-grid ("chunk")
regenerates identically no matter what order chunks are requested in.

The world model this implements is deliberately cheap and presentation-grade
(see ``docs/environment-generation.md`` and Rauken-Archipelago
``docs/world-generation.md``): "display fidelity beats internal fidelity". It is
**not** a geologically correct simulation -- there is no plate history, no
erosion solver and no sediment model. It produces *observable* terrain whose
relationships (upland ridges, drowned valleys / rias, shelves, buttes, ravines,
plains) read correctly at animal scale.

Sub-grid determinism contract
-----------------------------
``synthesize_field`` evaluates noise only at world-space coordinates
``x_m = origin_x + ix * cell_size``. Nothing in the per-cell arithmetic reads
the *local* loop index, so ``synthesize_field(params, seed, i0, j0, w, h)``
returns exactly the cells ``[i0:i0+w, j0:j0+h]`` of
``synthesize_field(params, seed, 0, 0, full_width, full_height)``. That is what
makes chunked generation order-independent.
"""

from __future__ import annotations

import math
import struct
from dataclasses import dataclass, field
from typing import Any, Sequence

from ..core.seeding import derive_seed

__all__ = [
    "Goldilocks",
    "MASK64",
    "MASK63",
    "splitmix64",
    "hash_unit",
    "value_noise",
    "fbm",
    "ridged",
    "warp2",
    "smootherstep",
    "smoothstep",
    "clamp",
    "HeightfieldParams",
    "Heightfield",
    "HeightfieldSynthesizer",
    "isolate_subgrid",
]

# --- deterministic integer hashing -----------------------------------------
_MASK64 = (1 << 64) - 1
_MASK63 = (1 << 63) - 1
_GOLDEN = 0x9E3779B97F4A7C15
_M1 = 0xBF58476D1CE4E5B9
_M2 = 0x94D049BB133111EB
_MASK64 = _MASK64
_INV2POW64 = 1.0 / float(1 << 64)


def splitmix64(z: int) -> int:
    """SplitMix64 finalizer. Pure integer arithmetic; stable across CPython builds."""
    z = (z + _GOLDEN) & _MASK64
    z = ((z ^ (z >> 30)) * _M1) & _MASK64
    z = ((z ^ (z >> 27)) * _M2) & _MASK64
    return (z ^ (z >> 31)) & _MASK64


def hash_unit(seed: int, *ints: int) -> float:
    """Map an integer lattice point to a float in ``[0, 1)``, stably.

    Not ``hash()``. A seeded, cross-platform deterministic hash.
    """
    h = seed & _MASK64
    for value in ints:
        h = splitmix64(h ^ ((int(value) * _GOLDEN) & _MASK64))
    return h / float(1 << 64)


# --- scalar helpers ---------------------------------------------------------
def clamp(x: float, lo: float = 0.0, hi: float = 1.0) -> float:
    return lo if x < lo else hi if x > hi else x


def smootherstep(t: float) -> float:
    """Ken Perlin's 6t^5-15t^4+10t^3 fade with zero 1st and 2nd derivatives."""
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def smoothstep(edge0: float, edge1: float, x: float) -> float:
    if edge1 == edge0:
        return 0.0 if x < edge0 else 1.0
    t = clamp((x - edge0) / (edge1 - edge0))
    return smootherstep(t)


# --- noise primitives -------------------------------------------------------
def value_noise(x: float, y: float, seed: int) -> float:
    """2D value noise in ``[0, 1)`` with a smootherstep interpolation.

    "Value / simplex-ish": cheap lattice value noise. It is smooth and
    decorrelatable enough for terrain masks; it is not gradient (Perlin) noise
    and it is not a spectral model.

    Hot path: the corner hashes and the SplitMix64 finalizer are inlined so a
    full-octave heightfield stays affordable in pure Python. It stays a pure
    function of ``(x, y, seed)`` and identical across platforms/versions.
    """
    x0 = math.floor(x)
    y0 = math.floor(y)
    fx = x - x0
    fy = y - y0
    ix0 = int(x0)
    iy0 = int(y0)
    ux = fx * fx * fx * (fx * (fx * 6.0 - 15.0) + 10.0)
    uy = fy * fy * fy * (fy * (fy * 6.0 - 15.0) + 10.0)
    s = seed & _MASK64
    g = _GOLDEN
    m = _MASK64
    m1 = _M1
    m2 = _M2
    inv = _INV2POW64
    # four corner hashes (SplitMix64 finalizer, inlined)
    z = (s ^ ((ix0 * g) & m) ^ ((iy0 * m2) & m)) & m
    z = (z + g) & m; z = ((z ^ (z >> 30)) * m1) & m; z = ((z ^ (z >> 27)) * m2) & m
    v00 = (z ^ (z >> 31)) * inv
    z = (s ^ (((ix0 + 1) * g) & m) ^ ((iy0 * m2) & m)) & m
    z = (z + g) & m; z = ((z ^ (z >> 30)) * m1) & m; z = ((z ^ (z >> 27)) * m2) & m
    v10 = (z ^ (z >> 31)) * inv
    z = (s ^ ((ix0 * g) & m) ^ (((iy0 + 1) * m2) & m)) & m
    z = (z + g) & m; z = ((z ^ (z >> 30)) * m1) & m; z = ((z ^ (z >> 27)) * m2) & m
    v01 = (z ^ (z >> 31)) * inv
    z = (s ^ (((ix0 + 1) * g) & m) ^ (((iy0 + 1) * m2) & m)) & m
    z = (z + g) & m; z = ((z ^ (z >> 30)) * m1) & m; z = ((z ^ (z >> 27)) * m2) & m
    v11 = (z ^ (z >> 31)) * inv
    a = v00 + (v10 - v00) * ux
    b = v01 + (v11 - v01) * ux
    return a + (b - a) * uy


def fbm(x: float, y: float, seeds: Sequence[int], *, lacunarity: float = 2.0,
        gain: float = 0.5, base_frequency: float = 1.0) -> float:
    """Fractional Brownian motion of value noise, normalized to ``[-1, 1]``."""
    amp = 1.0
    freq = base_frequency
    total = 0.0
    norm = 0.0
    for s in seeds:
        total += amp * (value_noise(x * freq, y * freq, s) * 2.0 - 1.0)
        norm += amp
        amp *= gain
        freq *= lacunarity
    return total / norm if norm else 0.0


def ridged(x: float, y: float, seeds: Sequence[int], *, lacunarity: float = 2.0,
           gain: float = 0.5, base_frequency: float = 1.0) -> float:
    """Ridged multifractal noise in ``[0, 1]`` -- sharp crests for ridges/needles."""
    amp = 1.0
    freq = base_frequency
    total = 0.0
    norm = 0.0
    weight = 1.0
    for s in seeds:
        n = 1.0 - abs(value_noise(x * freq, y * freq, s) * 2.0 - 1.0)
        n *= n
        total += amp * n * weight
        norm += amp
        weight = clamp(n * 2.0)
        amp *= gain
        freq *= lacunarity
    return total / norm if norm else 0.0


def warp2(x: float, y: float, wx_seed: int, wy_seed: int, *, strength: float,
          base_frequency: float) -> tuple[float, float]:
    """Domain warp: displace the sample point by a low-frequency noise offset."""
    wx = (value_noise(x * base_frequency + 5.2, y * base_frequency + 1.3, wx_seed) * 2.0 - 1.0) * strength
    wy = (value_noise(x * base_frequency + 9.2, y * base_frequency + 7.7, wy_seed) * 2.0 - 1.0) * strength
    return x + wx, y + wy


# --- parameters -------------------------------------------------------------
@dataclass
class HeightfieldParams:
    """World-space heightfield parameters (metres). Maps to the terrain
    definition's ``heightfield`` object."""

    width: int = 256
    height: int = 256
    cell_size: float = 4.0
    sea_level: float = 0.0
    origin_x: float = 0.0
    origin_y: float = 0.0

    relief_m: float = 200.0            # signed land/sea base relief scale
    mountains_relief_m: float = 150.0  # inland ridged mass
    detail_relief_m: float = 6.0       # fine everywhere
    deep_sea_m: float = 120.0          # seabed clamp depth below sea level

    continental_scale_m: float = 800.0
    continental_octaves: int = 1
    land_bias: float = 0.0             # <0 more ocean, >0 more land

    mountain_octaves: int = 5
    mountain_lacunarity: float = 2.0
    mountain_gain: float = 0.5
    ridged_relief: bool = True

    detail_octaves: int = 4
    detail_scale_m: float = 12.0

    warp_strength_m: float = 40.0
    warp_scale_m: float = 260.0

    #: optional radial island dome: {'cx_m','cy_m','radius_m','falloff_m','inner'}
    island: dict[str, float] | None = None

    @classmethod
    def from_dict(cls, data: dict[str, Any] | None) -> "HeightfieldParams":
        data = data or {}
        fields = {f: data[f] for f in cls.__dataclass_fields__ if f in data}
        return cls(**fields)

    def to_dict(self) -> dict[str, Any]:
        out: dict[str, Any] = {}
        for name in self.__dataclass_fields__:
            out[name] = getattr(self, name)
        return out


# --- the grid ---------------------------------------------------------------
@dataclass
class Heightfield:
    """A row-major float heightfield in metres, sea level at ``sea_level``."""

    width: int
    height: int
    cell_size: float
    sea_level: float
    origin_x: float = 0.0
    origin_y: float = 0.0
    data: list[float] = field(default_factory=list)

    @property
    def size_cells(self) -> int:
        return self.width * self.height

    def idx(self, ix: int, iy: int) -> int:
        return iy * self.width + ix

    def get(self, ix: int, iy: int) -> float:
        return self.data[iy * self.width + ix]

    def set(self, ix: int, iy: int, value: float) -> None:
        self.data[iy * self.width + ix] = value

    def world_xy(self, ix: int, iy: int) -> tuple[float, float]:
        return (self.origin_x + ix * self.cell_size,
                self.origin_y + iy * self.cell_size)

    def extent_m(self) -> tuple[float, float]:
        return (self.width * self.cell_size, self.height * self.cell_size)

    def sample(self, x_m: float, y_m: float) -> float:
        """Bilinear sample at world-space metres (clamped at edges)."""
        fx = (x_m - self.origin_x) / self.cell_size
        fy = (y_m - self.origin_y) / self.cell_size
        if fx < 0 or fy < 0 or fx > self.width - 1 or fy > self.height - 1:
            ix = min(max(int(round(fx)), 0), self.width - 1)
            iy = min(max(int(round(fy)), 0), self.height - 1)
            return self.get(ix, iy)
        ix = int(fx)
        iy = int(fy)
        tx = fx - ix
        ty = fy - iy
        ix2 = min(ix + 1, self.width - 1)
        iy2 = min(iy + 1, self.height - 1)
        a = self.get(ix, iy) + (self.get(ix2, iy) - self.get(ix, iy)) * tx
        b = self.get(ix, iy2) + (self.get(ix2, iy2) - self.get(ix, iy2)) * tx
        return a + (b - a) * ty

    # --- deterministic binary io -------------------------------------------
    _MAGIC = b"STHF"
    _VERSION = 1
    _HEADER_FMT = "<4sBiidddd"

    def to_bytes(self) -> bytes:
        n = len(self.data)
        header = struct.pack(
            self._HEADER_FMT, self._MAGIC, self._VERSION, self.width, self.height,
            self.cell_size, self.sea_level, self.origin_x, self.origin_y,
        )
        body = struct.pack("<%dd" % n, *self.data)
        return header + body

    @classmethod
    def from_bytes(cls, blob: bytes) -> "Heightfield":
        header_size = struct.calcsize(cls._HEADER_FMT)
        magic, version, w, h, cell, sea, ox, oy = struct.unpack_from(cls._HEADER_FMT, blob, 0)
        if magic != cls._MAGIC:
            raise ValueError("not a STHF heightfield blob")
        if version != cls._VERSION:
            raise ValueError(f"unsupported heightfield version {version}")
        # the heightfield is little-endian IEEE-754 float64, so byteswap on
        # big-endian hosts to keep the on-disk form identical everywhere.
        need_swap = __import__("sys").byteorder == "big"
        values = list(struct.unpack_from("<%dd" % (w * h), blob, header_size))
        if need_swap:  # pragma: no cover - only on big-endian hosts
            for i in range(len(values)):
                values[i] = struct.unpack("<d", struct.pack(">d", values[i]))[0]
        hf = cls(width=w, height=h, cell_size=cell, sea_level=sea, origin_x=ox, origin_y=oy, data=values)
        return hf

    def slice(self, i0: int, j0: int, w: int, h: int) -> "Heightfield":
        out = Heightfield(width=w, height=h, cell_size=self.cell_size, sea_level=self.sea_level,
                          origin_x=self.origin_x + i0 * self.cell_size,
                          origin_y=self.origin_y + j0 * self.cell_size)
        out.data = [
            self.get(i0 + lx, j0 + ly)
            for ly in range(h) for lx in range(w)
        ]
        return out

    # --- derived grids -----------------------------------------------------
    # `elevation` is the canonical height vector; `slope_degrees` is a derived
    # grid computed once per heightfield (deterministic, cached).

    @property
    def elevation(self) -> list[float]:
        return self.data

    @property
    def slope_degrees(self) -> list[float]:
        cached = getattr(self, "_slope_cache", None)
        if cached is not None and len(cached) == len(self.data):
            return cached
        w, h, cell = self.width, self.height, self.cell_size
        out: list[float] = [0.0] * (w * h)
        if w < 2 or h < 2 or cell <= 0.0:
            self._slope_cache = out
            return out
        for iy in range(h):
            y0 = max(iy - 1, 0)
            y1 = min(iy + 1, h - 1)
            dy = (y1 - y0) * cell
            row = iy * w
            for ix in range(w):
                x0 = max(ix - 1, 0)
                x1 = min(ix + 1, w - 1)
                dx = (x1 - x0) * cell
                gx = (self.get(x1, iy) - self.get(x0, iy)) / dx
                gy = (self.get(ix, y1) - self.get(ix, y0)) / dy
                out[row + ix] = math.degrees(math.atan(math.hypot(gx, gy)))
        object.__setattr__(self, "_slope_cache", out)
        return out

    def stats(self) -> dict[str, float]:
        if not self.data:
            return {"min": 0.0, "max": 0.0, "mean": 0.0, "land_fraction": 0.0, "cells": 0}
        lo = min(self.data)
        hi = max(self.data)
        mean = math.fsum(self.data) / len(self.data)
        land = sum(1 for v in self.data if v > self.sea_level)
        return {
            "min": lo, "max": hi, "mean": mean,
            "land_fraction": land / len(self.data), "cells": len(self.data),
        }


# --- the island-subgrid helper ---------------------------------------------
Goldilocks = Heightfield  # backwards-compatible alias (unused today)


def isolate_subgrid(hf: Heightfield, i0: int, j0: int, w: int, h: int) -> Heightfield:
    return hf.slice(i0, j0, w, h)


# --- the synthesizer --------------------------------------------------------
class HeightfieldSynthesizer:
    """Compose continental mask + shelf + ridged/fractal relief + domain warp."""

    def __init__(self, params: HeightfieldParams, seed: int):
        self.params = params
        self.seed = seed & _MASK64
        p = params
        self.cont_seeds = [derive_seed(seed, "heightfield", "continental", o)
                           for o in range(max(1, p.continental_octaves))]
        self.mtn_seeds = [derive_seed(seed, "heightfield", "mountain", o)
                          for o in range(max(1, p.mountain_octaves))]
        self.det_seeds = [derive_seed(seed, "heightfield", "detail", o)
                          for o in range(max(1, p.detail_octaves))]
        self.warp_x_seed = derive_seed(seed, "heightfield", "warp", "x")
        self.warp_y_seed = derive_seed(seed, "heightfield", "warp", "y")

    def _cell_height(self, ix_world: int, iy_world: int) -> float:
        p = self.params
        x = p.origin_x + ix_world * p.cell_size
        y = p.origin_y + iy_world * p.cell_size

        # domain warp in metres (produces flowing, branching ria-like curves)
        wx, wy = warp2(x, y, self.warp_x_seed, self.warp_y_seed,
                       strength=p.warp_strength_m, base_frequency=1.0 / p.warp_scale_m)

        # continental landness: >0 land, <0 sea, coastline near 0
        land = fbm(wx, wy, self.cont_seeds,
                   base_frequency=1.0 / p.continental_scale_m) + p.land_bias
        land = clamp(land, -1.0, 1.0)

        if p.island is not None:
            isl = p.island
            dx = x - isl.get("cx_m", 0.0)
            dy = y - isl.get("cy_m", 0.0)
            d = math.hypot(dx, dy)
            radius = isl.get("radius_m", 1.0)
            falloff = isl.get("falloff_m", 1.0)
            inner = 1.0 - smoothstep(radius * isl.get("inner", 0.6), radius, d)
            # inside the dome keep continental detail, outside force to sea shelf
            land = land * inner - (1.0 - inner)

        # base elevation (land positive, ocean negative, shelf near coast)
        elev = p.relief_m * land

        # inland ridged/fractal relief, only over solid land
        if p.ridged_relief:
            mtn = ridged(wx, wy, self.mtn_seeds,
                         lacunarity=p.mountain_lacunarity, gain=p.mountain_gain,
                         base_frequency=1.0 / (p.continental_scale_m * 0.35))
        else:
            mtn = fbm(wx, wy, self.mtn_seeds,
                      lacunarity=p.mountain_lacunarity, gain=p.mountain_gain,
                      base_frequency=1.0 / (p.continental_scale_m * 0.35)) * 0.5 + 0.5
        inland_weight = smoothstep(0.0, 0.25, land)
        elev += p.mountains_relief_m * inland_weight * mtn

        # fine detail everywhere (breaks flat plains and shelves)
        detail = fbm(x, y, self.det_seeds, base_frequency=1.0 / p.detail_scale_m)
        elev += p.detail_relief_m * detail

        if elev < p.sea_level - p.deep_sea_m:
            elev = p.sea_level - p.deep_sea_m
        return elev

    def synthesize_field(self, i0: int = 0, j0: int = 0,
                         w: int | None = None, h: int | None = None) -> Heightfield:
        p = self.params
        w = p.width if w is None else w
        h = p.height if h is None else h
        hf = Heightfield(
            width=w, height=h, cell_size=p.cell_size, sea_level=p.sea_level,
            origin_x=p.origin_x + i0 * p.cell_size, origin_y=p.origin_y + j0 * p.cell_size,
        )
        data = [0.0] * (w * h)
        # NOTE: world coordinates use the *global* origin + global cell index so
        # a sub-grid is bit-identical to the matching slice of the full grid.
        for ly in range(h):
            iy_world = j0 + ly
            row = ly * w
            for lx in range(w):
                data[row + lx] = self._cell_height(i0 + lx, iy_world)
        hf.data = data
        return hf

    def synthesize(self) -> Heightfield:
        return self.synthesize_field()


def synthesize_field(params: HeightfieldParams, seed: int, i0: int = 0, j0: int = 0,
                     w: int | None = None, h: int | None = None) -> Heightfield:
    """Convenience: synthesize any rectangular sub-grid deterministically."""
    return HeightfieldSynthesizer(params, seed).synthesize_field(i0, j0, w, h)
