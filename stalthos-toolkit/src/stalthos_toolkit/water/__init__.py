"""Water optics for The Stalthosian Archipelago.

Blender-independent, network-free, pure-stdlib water appearance maths:
Beer-Lambert transmittance, wavelength-dependent absorption and scattering,
wet-surface darkening / roughness, foam, glint band selection from sea state,
and a display-led lighting response.

Every function is a closed-form approximation chosen for how it *looks* under
the target rig (GTX 1080, 30 FPS floor), not for physical fidelity -- see
``docs/toolkit-architecture.md``. Nothing here reads the clock, the network,
the filesystem or any RNG except where the function is explicitly a pure
function of its arguments.

Re-exports the public surface of :mod:`stalthos_toolkit.water.optics` so callers
can ``from stalthos_toolkit.water import depth_ramp_rgb``.
"""

from __future__ import annotations

from .optics import (
    IOR_AIR,
    IOR_SEAWATER,
    IOR_WATER,
    PURE_WATER_ABSORPTION_M,
    PURE_WATER_SCATTERING_M,
    RAIN_RESPONSE,
    SCATTERING_WAVELENGTH_EXPONENT,
    SUN_GLINT_BANDS,
    SUN_GLINT_LABELS,
    SUN_GLINT_ROUGHNESS_MAX,
    TURBIDITY_TO_SCATTERING,
    WATER_CLASSES,
    absorption_coefficient_m,
    beer_lambert_rgb,
    beer_lambert_transmittance,
    depth_ramp_rgb,
    extinction_coefficient_m,
    foam_amount,
    fresnel_air_water,
    fresnel_schlick,
    fresnel_water_air,
    lighting_response,
    optics_table,
    scattering_coefficient_m,
    sea_state_glint,
    single_scattering_albedo,
    wetness_darkening,
    wetness_roughness,
)

__all__ = [
    "IOR_AIR",
    "IOR_SEAWATER",
    "IOR_WATER",
    "PURE_WATER_ABSORPTION_M",
    "PURE_WATER_SCATTERING_M",
    "RAIN_RESPONSE",
    "SCATTERING_WAVELENGTH_EXPONENT",
    "SUN_GLINT_BANDS",
    "SUN_GLINT_LABELS",
    "SUN_GLINT_ROUGHNESS_MAX",
    "TURBIDITY_TO_SCATTERING",
    "WATER_CLASSES",
    "absorption_coefficient_m",
    "beer_lambert_transmittance",
    "beer_lambert_rgb",
    "depth_ramp_rgb",
    "extinction_coefficient_m",
    "fresnel_air_water",
    "fresnel_schlick",
    "fresnel_water_air",
    "foam_amount",
    "lighting_response",
    "optics_table",
    "scattering_coefficient_m",
    "sea_state_glint",
    "single_scattering_albedo",
    "wetness_darkening",
    "wetness_roughness",
]