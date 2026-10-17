"""Water optics -- physically grounded, presentation-tuned.

This module is deliberately bpy-free: every water optical parameter is a plain
Python constant or a pure function over them. That makes the numbers unit
testable without Blender, diffable in review, and serialisable into the shader
graphs built by :mod:`stalthos_toolkit.water.shaders`.

Number discipline
-----------------
Every constant carries a `source` comment and an explicit
`artistic_override` note wherever the value is tuned for presentation rather
than taken from a measurement. Three tiers are used and never conflated:

  * VERIFIED    -- the source was actually fetched and read during this task.
                   The retrieval URL and date are recorded in `SOURCE_NOTES`
                   and repeated per-table.
  * REFERENCED  -- a real, well-known publication cited for a structural or
                   qualitative fact, but no number was re-measured against it.
                   Such numbers are always marked ARTISTIC.
  * ARTISTIC   -- chosen for how the archipelago should read. Never presented
                   as a measurement.

Nothing here is a fluid-dynamics, tidal or wave-propagation solve. See
`docs/water-and-shorelines.md` for what is deliberately *not* simulated.
"""

from __future__ import annotations

import math
from typing import Any, Iterable, Mapping, Sequence

__all__ = [
    "WAVELENGTH_BANDS",
    "SOURCE_NOTES",
    "PURE_WATER_ABSORPTION_M",
    "CLEAREST_NATURAL_WATER_ABSORPTION_M",
    "WATER_CLASSES",
    "JERLOV_WATER_CLASSES",
    "WATER_CLASS_SCALE",
    "SCATTERING_ALBEDO",
    "IOR_AIR",
    "IOR_WATER",
    "IOR_SEAWATER",
    "F0_AIR_WATER",
    "CRITICAL_ANGLE_WATER_AIR_RAD",
    "SEAWATER_SALINITY_PSU",
    "SCATTERING_WAVELENGTH_EXPONENT",
    "PURE_WATER_SCATTERING_M",
    "TURBIDITY_UNITS",
    "TURBIDITY_TO_SCATTERING",
    "SUN_GLINT_BANDS",
    "SUN_GLINT_LABELS",
    "DEPTH_RAMP",
    "WET_SAND",
    "DRYING",
    "FOAM_RULES",
    "SHALLOW_SSS",
    "LIGHTING",
    "absorption_coefficient_m",
    "extinction_coefficient_m",
    "scattering_coefficient_m",
    "single_scattering_albedo",
    "beer_lambert_transmittance",
    "beer_lambert_rgb",
    "fresnel_air_water",
    "fresnel_water_air",
    "fresnel_schlick",
    "depth_ramp_rgb",
    "wetness_darkening",
    "foam_amount",
    "sea_state_glint",
    "lighting_response",
    "optics_table",
]

# --- provenance -------------------------------------------------------------
#: Every VERIFIED number in this module traces to one of these retrievals.
#: Re-fetching these URLs and re-reading the tables is the regression test.
SOURCE_NOTES = {
    "pope97": {
        "citation": (
            "R. M. Pope and E. S. Fry, \"Absorption spectrum (380-700 nm) of "
            "pure water. II. Integrating cavity measurements,\" Appl. Opt. 36, "
            "8710-8723 (1997)."
        ),
        "url": "https://omlc.org/spectra/water/data/pope97.txt",
        "retrieved": "2026-10-10",
        "verified": True,
        "what": (
            "Tabulated a(lambda) in 1/cm from 380 to 727.5 nm at 2.5 nm steps. "
            "Values here are those numbers x100 (m^-1), decimated to 5 nm. "
            "Pope & Fry's own abstract reports the minimum 0.0044 +/- 0.0006 "
            "m^-1 at 418 nm; this table reproduces 0.00442 m^-1 at 417.5 nm."
        ),
    },
    "smith81": {
        "citation": (
            "R. C. Smith and K. S. Baker, \"Optical properties of the clearest "
            "natural waters (200-800 nm),\" Appl. Opt. 20, 177-184 (1981)."
        ),
        "url": "https://omlc.org/spectra/water/data/smith81.txt",
        "retrieved": "2026-10-10",
        "verified": True,
        "what": (
            "Tabulated a(lambda) in 1/cm for the clearest natural waters, "
            "200-800 nm. Used only as the 'Case-1 clear' reference curve that "
            "sits above pure water; it is NOT used as the archipelago default."
        ),
    },
    "morel77": {
        "citation": (
            "A. Morel and L. Prieur, \"Analysis of variations in ocean color,\" "
            "Limnol. Oceanogr. 22, 709-722 (1977)."
        ),
        "url": "https://omlc.org/spectra/water/data/morel77.txt",
        "retrieved": "2026-10-10",
        "verified": True,
        "what": (
            "The Case-1 / Case-2 conceptual split. Cited for the *classification* "
            "idea, not for any coefficient."
        ),
    },
    "jerlov76": {
        "citation": (
            "N. G. Jerlov, \"Marine Optics,\" Elsevier Oceanography Series 14 "
            "(1976)."
        ),
        "url": None,
        "retrieved": None,
        "verified": False,
        "what": (
            "Water-type classification (oceanic I/IA/IB/II/III, coastal 1/3/5/7/9). "
            "Cited for the type names and the clear->turbid ordering ONLY. "
            "No numeric Kd in this module is taken from Jerlov: every Kd value "
            "below is ARTISTIC, calibrated against Pope & Fry + Smith & Baker."
        ),
    },
    "mobley94": {
        "citation": (
            "C. D. Mobley, \"Light and Water: Radiative Transfer in Natural "
            "Waters,\" Academic Press (1994)."
        ),
        "url": None,
        "retrieved": None,
        "verified": False,
        "what": (
            "Scattering phase function (Fournier-Forand / Petzold), single-scattering "
            "albedo as a turbidity proxy, and the Cox-Munk surface-slope variance "
            "model used by SUN_GLINT_BANDS. Cited structurally; the numeric bands "
            "below are ARTISTIC fits to that shape, not Mobley's tables."
        ),
    },
}

# --- wavelengths ------------------------------------------------------------
#: The three bands the generated shaders actually evaluate. A real renderer may
#: run more bands; these are the minimum set that separates "red dies first"
#: (a=0.268 m^-1 at 615 nm) from "blue penetrates" (a=0.010 m^-1 at 465 nm).
#: ARTISTIC: a three-band RGB model is a display choice, not physics.
WAVELENGTH_BANDS = {
    "red": 615,
    "green": 535,
    "blue": 465,
}

#: Pure-water absorption a(lambda), m^-1, decimated to 5 nm from Pope & Fry 1997.
#: VERIFIED (see SOURCE_NOTES["pope97"]). This is the anchor of the whole model:
#: it is why warm shallow water reads blue-green and why depth desaturates to navy.
PURE_WATER_ABSORPTION_M = {
    380: 0.01137, 385: 0.00941, 390: 0.00851, 395: 0.00813, 400: 0.00663,
    405: 0.0053, 410: 0.00473, 415: 0.00444, 420: 0.00454, 425: 0.00478,
    430: 0.00495, 435: 0.0053, 440: 0.00635, 445: 0.00751, 450: 0.00922,
    455: 0.00962, 460: 0.00979, 465: 0.01011, 470: 0.0106, 475: 0.0114,
    480: 0.0127, 485: 0.0136, 490: 0.015, 495: 0.0173, 500: 0.0204,
    505: 0.0256, 510: 0.0325, 515: 0.0396, 520: 0.0409, 525: 0.0417,
    530: 0.0434, 535: 0.0452, 540: 0.0474, 545: 0.0511, 550: 0.0565,
    555: 0.0596, 560: 0.0619, 565: 0.0642, 570: 0.0695, 575: 0.0772,
    580: 0.0896, 585: 0.11, 590: 0.1351, 595: 0.1672, 600: 0.2224,
    605: 0.2577, 610: 0.2644, 615: 0.2678, 620: 0.2755, 625: 0.2834,
    630: 0.2916, 635: 0.3012, 640: 0.3108, 645: 0.325, 650: 0.34,
    655: 0.371, 660: 0.41, 665: 0.429, 670: 0.439, 675: 0.448,
    680: 0.465, 685: 0.486, 690: 0.516, 695: 0.559, 700: 0.624,
    725: 1.489,
}

#: Smith & Baker (1981) clearest natural waters, decimated to 5 nm where the
#: source table allows. VERIFIED. Kept as a *reference curve*: it is what real
#: open water looks like once phytoplankton and detritus are present, i.e. it
#: sits above Pope & Fry at blue wavelengths (0.0145 vs 0.00635 m^-1 at 440 nm,
#: a 2.3x ratio) and converges toward it in the red.
CLEAREST_NATURAL_WATER_ABSORPTION_M = {
    380: 0.022, 385: 0.018, 390: 0.0191, 395: 0.0177, 400: 0.0171,
    405: 0.016, 410: 0.0153, 415: 0.0149, 420: 0.015, 425: 0.0149,
    430: 0.0146, 435: 0.0144, 440: 0.0145, 445: 0.0146, 450: 0.015,
    455: 0.0152, 460: 0.0156, 465: 0.0162, 470: 0.0166, 475: 0.0171,
    480: 0.0176, 485: 0.0185, 490: 0.0195, 495: 0.0214, 500: 0.0257,
    505: 0.0304, 510: 0.0356, 515: 0.0412, 520: 0.0477, 525: 0.0548,
    530: 0.060, 535: 0.0631, 540: 0.0638, 545: 0.0638, 550: 0.0638,
    555: 0.0657, 560: 0.0708, 565: 0.0775, 570: 0.083, 575: 0.0925,
    580: 0.108, 585: 0.123, 590: 0.148, 595: 0.181, 600: 0.244,
    605: 0.286, 610: 0.318, 615: 0.338, 620: 0.309, 625: 0.315,
    630: 0.323, 635: 0.329, 640: 0.329, 645: 0.34, 650: 0.365,
    655: 0.385, 660: 0.4, 665: 0.42, 670: 0.445, 675: 0.46,
    680: 0.45, 685: 0.46, 690: 0.48, 695: 0.55, 700: 0.65,
}

#: The master water-class registry.
#:
#: `kd_450_m` is the diffuse attenuation coefficient at 450 nm in m^-1 and
#: `scale` multiplies PURE_WATER_ABSORPTION_M. `scattering_albedo` is the
#: single-scattering albedo b/(a+b) at 535 nm.
#:
#: `family: "marine"` classes carry a Jerlov type; `family: "freshwater"`
#: classes do not -- the Jerlov classification is marine-only, so a freshwater
#: class carrying a Jerlov number would be a category error.
#:
#: REFERENCED for the Jerlov type names and ordering (Jerlov 1976); every numeric
#: value is ARTISTIC. `verified: false` is deliberate and load-bearing: do not
#: quote these Kd or albedo values as measurements.
WATER_CLASSES = {
    "open_ocean_clear": {
        "family": "marine",
        "jerlov_type": "I",
        "label": "Clear oceanic",
        "kd_450_m": 0.035,
        "scale": 1.6,
        "scattering_albedo": 0.35,
        "verified": False,
        "note": "ARTISTIC: kd ~= a_pure(450) * scale + scattering.",
    },
    "open_ocean_typical": {
        "family": "marine",
        "jerlov_type": "II",
        "label": "Typical oceanic",
        "kd_450_m": 0.06,
        "scale": 2.4,
        "scattering_albedo": 0.5,
        "verified": False,
        "note": "ARTISTIC.",
    },
    "coastal_clear": {
        "family": "marine",
        "jerlov_type": "1",
        "label": "Clear coastal",
        "kd_450_m": 0.11,
        "scale": 4.0,
        "scattering_albedo": 0.65,
        "verified": False,
        "note": "ARTISTIC.",
    },
    "coastal_turbid": {
        "family": "marine",
        "jerlov_type": "3",
        "label": "Turbid coastal",
        "kd_450_m": 0.22,
        "scale": 8.0,
        "scattering_albedo": 0.85,
        "verified": False,
        "note": "ARTISTIC.",
    },
    "estuarine": {
        "family": "marine",
        "jerlov_type": "5",
        "label": "Estuarine / brackish",
        "kd_450_m": 0.45,
        "scale": 14.0,
        "scattering_albedo": 0.9,
        "verified": False,
        "note": "ARTISTIC. CDOM (gilvin) dominates: absorption is highest in the blue.",
    },
    "river_fresh": {
        "family": "freshwater",
        "jerlov_type": None,
        "label": "River / stream",
        "kd_450_m": 0.30,
        "scale": 10.0,
        "scattering_albedo": 0.55,
        "verified": False,
        "note": "ARTISTIC. Fresh but carrying sediment; greyer and less blue than the sea.",
    },
    "wetland_still": {
        "family": "freshwater",
        "jerlov_type": None,
        "label": "Pond / wetland, still",
        "kd_450_m": 0.55,
        "scale": 16.0,
        "scattering_albedo": 0.8,
        "verified": False,
        "note": "ARTISTIC. Tannin- and sediment-laden; the brown end of the scale.",
    },
}

#: The marine subset, keyed by Jerlov type where one exists.
JERLOV_WATER_CLASSES = {name: spec for name, spec in WATER_CLASSES.items()
                         if spec["family"] == "marine"}

#: Multiplicative scale applied to the absorption table per water class.
#: Kept separate from `kd_450_m` so the *spectral shape* stays Pope & Fry's and
#: only the magnitude moves -- which is what real turbidity does (it adds a
#: blue-weighted absorber/CDOM term) rather than shifting the whole curve.
WATER_CLASS_SCALE = {name: spec["scale"] for name, spec in WATER_CLASSES.items()}

#: The single-scattering albedo per water class.
SCATTERING_ALBEDO = {name: spec["scattering_albedo"] for name, spec in WATER_CLASSES.items()}

# --- refractive index / Fresnel ---------------------------------------------
#: VERIFIED-structural (index of refraction is a tabulated physical property; the
#: value 1.333 for water at visible wavelengths and 20 C is standard reference
#: data, e.g. Hale & Querry 1973, which is cited by Pope & Fry 1997). ARTISTIC:
#: a single constant instead of a dispersion curve -- the toolkit's three bands
#: do not resolve the difference between n(465 nm) and n(615 nm).
IOR_AIR = 1.0003
IOR_WATER = 1.333
#: Seawater at 35 PSU is ~1.340. REFERENCED (standard table value), ARTISTIC in
#: that we ignore the salinity dispersion curve.
IOR_SEAWATER = 1.340
#: Practical salinity of the archipelago's open water. ARTISTIC world-building
#: choice: tropical shallow sea, not a measurement.
SEAWATER_SALINITY_PSU = 35.0

#: Normal-incidence reflectance, air->water. Exact: ((n2-n1)/(n2+n1))^2.
#: This is the number that makes water read as water at steep view angles --
#: ~2% reflectance looking straight down, so refraction and in-water colour carry
#: the near-nadir image, and ~100% at grazing.
F0_AIR_WATER = ((IOR_WATER - IOR_AIR) / (IOR_WATER + IOR_AIR)) ** 2

#: Total-internal-reflection critical angle for a water->air interface.
#: VERIFIED-structural (Snell's law; the arithmetic is checked by tests).
CRITICAL_ANGLE_WATER_AIR_RAD = math.asin(IOR_AIR / IOR_WATER)

# --- scattering / turbidity --------------------------------------------------
#: Wavelength exponent of the scattering phase function's large-particle part.
#: REFERENCED: pure-water Rayleigh-like scattering is ~lambda^-4.3 (Morel 1974,
#: quoted in Mobley 1994); coastal particle scattering is much flatter,
#: lambda^-1 to lambda^0. ARTISTIC: a single exponent for the whole model,
#: chosen between the two.
SCATTERING_WAVELENGTH_EXPONENT = 4.0

#: Pure-water scattering coefficient at 535 nm, m^-1. ARTISTIC: I could not
#: retrieve a machine-readable Morel (1974) table, so this is calibrated so that
#: the single-scattering albedo at 535 nm comes out plausible against the
#: VERIFIED absorption (0.0452 m^-1). Consequence: the *shape* follows
#: lambda^-4.0, the absolute scale is presentation-tuned. See docs.
PURE_WATER_SCATTERING_M = 0.0025

#: Turbidity units the definition file may use, and how to convert.
#: REFERENCED: NTU (nephelometric turbidity units) is the field standard
#: (EPA Method 180.1). FTU is treated as numerically equal to NTU here --
#: ARTISTIC simplification, the two are not strictly interchangeable.
TURBIDITY_UNITS = ("ntu", "ftu", "mg_l_ss")

#: Scattering added per unit turbidity at 535 nm, m^-1 per NTU.
#: ARTISTIC: calibrated so that 10 NTU roughly doubles clear-water scattering,
#: which keeps a muddy river visibly milkier without turning it opaque.
TURBIDITY_TO_SCATTERING = {"ntu": 0.012, "ftu": 0.012, "mg_l_ss": 0.35}

# --- sun glint / sea state ---------------------------------------------------
#: Specular roughness bands by sea state, plus the Cox-Munk slope-variance scale.
#: Each band is a tangent-space roughness for a Blinn/Phong-ish specular lobe.
#: REFERENCED: Cox & Munk (1954) wind-wave slope variance, as summarised in
#: Mobley 1994 and the Ocean Optics Web Book "Sea Surface Cox-Munk". ARTISTIC:
#: the band edges and the roughness numbers are presentation fits to that
#: model's shape (slope variance ~ (0.003 + 0.00512 * U10)), not a lookup.
SUN_GLINT_BANDS = {
    # (beaufort_name, wind_ms, slope_variance, roughness, normal_scale)
    0: {"beaufort_name": "glassy calm", "wind_ms": 0.5, "slope_variance": 0.003,
        "roughness": 0.005, "normal_scale": 0.05},
    1: {"beaufort_name": "light air", "wind_ms": 1.5, "slope_variance": 0.0038,
        "roughness": 0.010, "normal_scale": 0.12},
    2: {"beaufort_name": "light breeze", "wind_ms": 3.0, "slope_variance": 0.0056,
        "roughness": 0.022, "normal_scale": 0.28},
    3: {"beaufort_name": "gentle breeze", "wind_ms": 5.5, "slope_variance": 0.0074,
        "roughness": 0.040, "normal_scale": 0.50},
    4: {"beaufort_name": "moderate breeze", "wind_ms": 8.0, "slope_variance": 0.0103,
        "roughness": 0.070, "normal_scale": 0.85},
    5: {"beaufort_name": "fresh breeze", "wind_ms": 10.5, "slope_variance": 0.0129,
        "roughness": 0.110, "normal_scale": 1.30},
    6: {"beaufort_name": "strong breeze", "wind_ms": 13.5, "slope_variance": 0.0165,
        "roughness": 0.165, "normal_scale": 1.90},
    7: {"beaufort_name": "near gale", "wind_ms": 16.5, "slope_variance": 0.0201,
        "roughness": 0.240, "normal_scale": 2.70},
    8: {"beaufort_name": "gale", "wind_ms": 19.5, "slope_variance": 0.0237,
        "roughness": 0.340, "normal_scale": 3.80},
    9: {"beaufort_name": "strong gale", "wind_ms": 22.5, "slope_variance": 0.0273,
        "roughness": 0.470, "normal_scale": 5.20},
}

#: Short artist-facing name for each glint band, keyed by sea state 0..9.
#: ARTISTIC: these are *display* names for HUDs, layer names and material
#: variants -- not a meteorology vocabulary. The canonical WMO Beaufort names
#: stay in `SUN_GLINT_BANDS[state]["beaufort_name"]`; do not conflate the two.
SUN_GLINT_LABELS = {
    0: "glass",
    1: "ripples",
    2: "cat's paws",
    3: "chop",
    4: "light chop",
    5: "waves",
    6: "sea",
    7: "swell",
    8: "rough sea",
    9: "high sea",
}

#: Clamp for the roughness the shader may actually receive. ARTISTIC: above ~0.5
#: a Blinn lobe on a plane stops reading as water and starts reading as metal.
SUN_GLINT_ROUGHNESS_MAX = 0.5

# --- depth ramp ---------------------------------------------------------------
#: Depths, in metres, at which the depth colour ramp is sampled, and the
#: absorption multiplier applied at each stop. ARTISTIC: the stops are chosen so
#: the ramp passes through the readable milestones (red gone, green half-gone,
#: blue still present) rather than at uniform intervals.
DEPTH_RAMP = {
    "max_depth_m": 25.0,
    "stops_m": [0.0, 0.5, 1.0, 2.0, 4.0, 8.0, 15.0, 25.0],
    #: Below this depth the water body is effectively opaque; the ramp clamps.
    "opaque_depth_m": 25.0,
    #: Depth at which red is reduced to 10% in clear water -- the "red is gone"
    #: milestone. ARTISTIC but checked against a(615)=0.2678 m^-1:
    #: -ln(0.1)/0.2678 = 8.58 m, so this is consistent, not arbitrary.
    "red_gone_depth_m": 8.58,
}

# --- wet sand / shoreline -----------------------------------------------------
WET_SAND = {
    #: Albedo multiplier for fully wet sand. ARTISTIC: measured wet-sand
    #: darkening in the sediment/remote-sensing literature varies from ~0.3 to
    #: ~0.8 depending on grain size and mineralogy; I could not retrieve a
    #: machine-readable table, so 0.55 is a presentation choice mid-range.
    "darkening_min": 0.55,
    #: Roughness of wet sand. REFERENCED conceptually: a water film makes the
    #: surface closer to a smooth dielectric, so roughness drops. ARTISTIC value.
    "roughness_wet": 0.18,
    "roughness_dry": 0.85,
    #: Specular reflectance of the thin water film on wet sand. ARTISTIC, in line
    #: with F0_AIR_WATER plus a roughness spread.
    "film_specular": 0.03,
    #: Capillary fringe width above the waterline, metres. ARTISTIC: chosen so the
    #: wet band reads at the Hero Beast's body scale.
    "capillary_fringe_m": 0.12,
    #: Darkening applied to the sand *inside* the capillary fringe (damp but not
    #: flooded). ARTISTIC.
    "damp_darkening": 0.75,
}

DRYING = {
    #: Seconds for a wet surface to return to `dry` after the water leaves.
    #: ARTISTIC: presentation time constant, not an evaporation model.
    "time_constant_s": 90.0,
    #: Shape of the drying curve: 1.0 = linear, <1 = fast then slow (the usual
    #: look: the sheen goes first, the damp stain lingers).
    "curve_shape": 0.45,
    #: Residual darkening that never dries out in a single session -- the stain.
    "residual_darkening": 0.92,
}

# --- foam ---------------------------------------------------------------------
FOAM_RULES = {
    #: Foam coverage 0..1 is a *display* quantity. The three generator rules
    #: below each produce a coverage in [0,1]; they are combined by
    #: `foam_amount`. Every field is bounded and the bounds are asserted by tests.
    "max_coverage": 1.0,
    "breaking_crest": {
        #: Wave height (m) at which the crest starts to break. ARTISTIC:
        #: real breaking depends on steepness (height/wavelength), bottom slope
        #: and wind; we collapse that to a height threshold plus a steepness term.
        "onset_height_m": 0.45,
        #: Steepness (height / wavelength) above which even small waves break.
        "onset_steepness": 0.09,
        #: Coverage at full breaking.
        "max_coverage": 0.85,
        #: Foam patchiness: 0 = smooth sheet, 1 = lace. ARTISTIC.
        "patchiness": 0.55,
        #: Foam decay time after breaking, seconds. ARTISTIC.
        "persistence_s": 2.5,
        #: Foam albedo. REFERENCED: fresh sea foam is bright, near-Lambertian,
        #: albedo ~0.7-0.9. ARTISTIC value in that range.
        "albedo": 0.8,
    },
    "shoreline_swash": {
        #: Shore slope (rise/run) below which swash cannot pile up foam.
        "max_slope": 0.35,
        #: Significant wave height needed to push foam up the beach, metres.
        "onset_height_m": 0.25,
        #: Foam band width normal to the shoreline, metres. ARTISTIC.
        "band_width_m": 1.2,
        #: Foam coverage at the band centre at full swash.
        "max_coverage": 0.6,
        #: How far the foam band decays away from the waterline. ARTISTIC.
        "decay_length_m": 0.6,
        "patchiness": 0.4,
        "persistence_s": 4.0,
        "albedo": 0.75,
    },
    "obstacle_wake": {
        #: Flow speed (m/s) below which no wake foam forms. ARTISTIC.
        "onset_speed_ms": 0.8,
        #: Flow speed at which wake foam saturates. ARTISTIC.
        "saturate_speed_ms": 3.0,
        #: Obstacle diameter (m) below which no wake foam forms. ARTISTIC.
        "onset_diameter_m": 0.25,
        #: Wake length behind the obstacle at saturation, metres. ARTISTIC.
        "wake_length_m": 4.0,
        "max_coverage": 0.5,
        "patchiness": 0.65,
        "persistence_s": 1.8,
        "albedo": 0.78,
    },
    #: Bubble size distribution scale (mm) for the foam normal/roughness.
    #: ARTISTIC: coarse foam reads as churned, fine foam as mist.
    "bubble_scale_mm": 2.0,
    #: Foam roughness (it is a diffuse bright surface, not a mirror).
    "roughness": 0.65,
}

# --- shallow subsurface -------------------------------------------------------
SHALLOW_SSS = {
    #: Subsurface scattering tint for water shallower than `max_depth_m`.
    #: ARTISTIC: the tint is the *complement* of what absorption removes -- warm
    #: shallow water glows slightly green-cyan because red is already attenuated
    #: and the remaining blue-green is re-emitted toward the eye.
    "tint_rgb": (0.55, 0.85, 0.80),
    #: Depth (m) below which SSS is switched off entirely.
    "min_depth_m": 0.05,
    #: Depth (m) above which the SSS contribution has faded to zero.
    "max_depth_m": 3.0,
    #: Strength of the SSS term at the shallow end. ARTISTIC.
    "strength": 0.35,
    #: Sand-bed albedo multiplier that brightens the SSS term -- a white sand
    #: bed bounces light back through the water column. ARTISTIC.
    "bed_albedo_boost": 0.25,
}

# --- lighting response ---------------------------------------------------------
#: How the water surface parameters shift under three illumination states.
#: `specular_gain` multiplies the sun-glint term, `roughness_offset` shifts the
#: glint band, `ambient_gain` lifts the sky-reflection term, `wet_gain` multiplies
#: the rain-wet response.
#: ARTISTIC throughout: these are the presentation knobs that keep the same
#: material readable in bright sun, flat overcast and after rain.
LIGHTING = {
    "clear_sky": {
        "specular_gain": 1.0,
        "roughness_offset": 0.0,
        "ambient_gain": 1.0,
        "wet_gain": 0.0,
        "exposure_ev": 0.0,
    },
    "overcast": {
        "specular_gain": 0.25,
        "roughness_offset": 0.06,
        "ambient_gain": 1.6,
        "wet_gain": 0.0,
        "exposure_ev": -0.4,
    },
    "wet": {
        "specular_gain": 0.7,
        "roughness_offset": -0.04,
        "ambient_gain": 1.25,
        "wet_gain": 1.0,
        "exposure_ev": -0.1,
    },
}

#: Rain response, kept separate from LIGHTING so the state dicts stay uniform.
#: ARTISTIC: rain dimples sharpen the glint without erasing it.
RAIN_RESPONSE = {
    "roughness_gain": 0.35,
    "rain_drop_height_mm": 1.2,
}


# ==== functions ==============================================================
def _table(table: Mapping[int, float], nm: float) -> float:
    """Linear interpolation over a {wavelength_nm: coefficient} table."""
    keys = sorted(table)
    if nm <= keys[0]:
        return float(table[keys[0]])
    if nm >= keys[-1]:
        return float(table[keys[-1]])
    for low, high in zip(keys, keys[1:]):
        if low <= nm <= high:
            span = high - low
            t = 0.0 if span == 0 else (nm - low) / span
            return float(table[low] * (1.0 - t) + table[high] * t)
    raise AssertionError("unreachable")


def absorption_coefficient_m(nm: float, water_class: str = "open_ocean_clear",
                            turbidity: float = 0.0, turbidity_unit: str = "ntu") -> float:
    """Total absorption a(nm) in m^-1 for a water class plus a turbidity term.

    `water_class` scales the Pope & Fry pure-water curve (see WATER_CLASS_SCALE);
    `turbidity` adds a blue-weighted empirical term (see TURBIDITY_TO_SCATTERING's
    companion in `scattering_coefficient_m`).
    """
    if water_class not in WATER_CLASS_SCALE:
        raise KeyError(f"unknown water class {water_class!r}; known: {sorted(WATER_CLASS_SCALE)}")
    if turbidity_unit not in TURBIDITY_TO_SCATTERING:
        raise KeyError(f"unknown turbidity unit {turbidity_unit!r}")
    base = _table(PURE_WATER_ABSORPTION_M, nm) * WATER_CLASS_SCALE[water_class]
    return base


def scattering_coefficient_m(nm: float, water_class: str = "open_ocean_clear",
                             turbidity: float = 0.0, turbidity_unit: str = "ntu") -> float:
    """Total scattering b(nm) in m^-1: a wavelength-shaped base plus turbidity.

    The base is PURE_WATER_SCATTERING_M re-weighted by
    SCATTERING_WAVELENGTH_EXPONENT and scaled toward the class's
    single-scattering albedo. Turbidity adds a *flat* (wavelength-independent)
    term, which is what suspended sediment actually does.
    """
    if water_class not in SCATTERING_ALBEDO:
        raise KeyError(f"unknown water class {water_class!r}")
    if turbidity_unit not in TURBIDITY_TO_SCATTERING:
        raise KeyError(f"unknown turbidity unit {turbidity_unit!r}")
    ref = float(WAVELENGTH_BANDS["green"])
    omega = SCATTERING_ALBEDO[water_class]
    # Solve b/(a+b) = omega for b at the reference wavelength, so the class's
    # declared albedo is exact at 535 nm and only the *shape* moves away from it.
    a_ref = absorption_coefficient_m(ref, water_class)
    a_nm = absorption_coefficient_m(nm, water_class)
    b_ref = a_ref * omega / max(1e-6, 1.0 - omega)
    # Re-shape by the scattering wavelength exponent, normalised by the class's
    # own absorption curve so turbid classes do not also get bluer.
    base = b_ref * (nm / ref) ** -SCATTERING_WAVELENGTH_EXPONENT * (a_nm / max(1e-6, a_ref))
    return base + turbidity * TURBIDITY_TO_SCATTERING[turbidity_unit]


def extinction_coefficient_m(nm: float, water_class: str = "open_ocean_clear",
                             turbidity: float = 0.0, turbidity_unit: str = "ntu") -> float:
    """c = a + b, the beam extinction coefficient in m^-1."""
    return absorption_coefficient_m(nm, water_class, turbidity, turbidity_unit) + \
        scattering_coefficient_m(nm, water_class, turbidity, turbidity_unit)


def single_scattering_albedo(nm: float, water_class: str = "open_ocean_clear",
                             turbidity: float = 0.0, turbidity_unit: str = "ntu") -> float:
    """omega_0 = b / (a + b) in [0, 1]."""
    c = extinction_coefficient_m(nm, water_class, turbidity, turbidity_unit)
    if c <= 0.0:
        return 0.0
    return scattering_coefficient_m(nm, water_class, turbidity, turbidity_unit) / c


def beer_lambert_transmittance(coeff_m: float, depth_m: float) -> float:
    """T = exp(-c * d). Monotonically decreasing in depth, in [0, 1]."""
    if coeff_m < 0.0:
        raise ValueError("extinction coefficient must be >= 0")
    return math.exp(-coeff_m * max(0.0, depth_m))


def beer_lambert_rgb(depth_m: float, water_class: str = "open_ocean_clear",
                    turbidity: float = 0.0, turbidity_unit: str = "ntu") -> tuple[float, float, float]:
    """Per-band transmittance through `depth_m` of water, as an RGB tuple."""
    out = []
    for band in ("red", "green", "blue"):
        a = absorption_coefficient_m(WAVELENGTH_BANDS[band], water_class, turbidity, turbidity_unit)
        out.append(beer_lambert_transmittance(a, depth_m))
    return (out[0], out[1], out[2])


def fresnel_air_water(cos_theta_i: float) -> float:
    """Exact unpolarized Fresnel reflectance for air -> water.

    `cos_theta_i` is the cosine of the incidence angle measured from the normal
    (1 = looking straight down). Rises from F0_AIR_WATER at normal incidence to
    1.0 at grazing. Values outside [-1, 1] are clamped.
    """
    c = min(1.0, max(-1.0, float(cos_theta_i)))
    n1, n2 = IOR_AIR, IOR_WATER
    sin2_t = (n1 / n2) ** 2 * (1.0 - c * c)
    if sin2_t >= 1.0:
        return 1.0
    cos_t = math.sqrt(1.0 - sin2_t)
    rs = (n1 * c - n2 * cos_t) / (n1 * c + n2 * cos_t)
    rp = (n1 * cos_t - n2 * c) / (n1 * cos_t + n2 * c)
    return 0.5 * (rs * rs + rp * rp)


def fresnel_water_air(cos_theta_i: float) -> float:
    """Exact unpolarized Fresnel reflectance for water -> air.

    Same normal-incidence value as `fresnel_air_water` (reciprocity for a lossless
    interface), but reaches 1.0 at CRITICAL_ANGLE_WATER_AIR_RAD instead of at
    grazing: beyond the critical angle the wave is totally internally reflected,
    which is why the underside of the surface is a mirror.
    """
    c = min(1.0, max(-1.0, float(cos_theta_i)))
    n1, n2 = IOR_WATER, IOR_AIR
    sin2_t = (n1 / n2) ** 2 * (1.0 - c * c)
    if sin2_t >= 1.0:
        return 1.0  # total internal reflection
    cos_t = math.sqrt(1.0 - sin2_t)
    rs = (n1 * c - n2 * cos_t) / (n1 * c + n2 * cos_t)
    rp = (n1 * cos_t - n2 * c) / (n1 * cos_t + n2 * c)
    return 0.5 * (rs * rs + rp * rp)


def fresnel_schlick(cos_theta: float, f0: float = F0_AIR_WATER) -> float:
    """Schlick's approximation: F0 + (1 - F0) * (1 - cos)^5.

    Cheaper than the exact form and visually indistinguishable for water over the
    angles that matter. Used by the shader graph's rough-reflection path.
    """
    c = min(1.0, max(0.0, float(cos_theta)))
    return f0 + (1.0 - f0) * (1.0 - c) ** 5


def depth_ramp_rgb(depth_m: float, water_class: str = "open_ocean_clear",
                   turbidity: float = 0.0, turbidity_unit: str = "ntu",
                   opaque_depth_m: float | None = None) -> tuple[float, float, float]:
    """Normalised in-water radiance multiplier at `depth_m`.

    Clamped to [0, 1] per band and to `opaque_depth_m` (default
    DEPTH_RAMP["opaque_depth_m"]) so a deep body does not produce negative colour.
    """
    limit = DEPTH_RAMP["opaque_depth_m"] if opaque_depth_m is None else opaque_depth_m
    d = min(max(0.0, float(depth_m)), max(0.0, float(limit)))
    return beer_lambert_rgb(d, water_class, turbidity, turbidity_unit)


def wetness_darkening(wetness: float) -> float:
    """Albedo multiplier for a surface at `wetness` in [0, 1].

    1.0 (dry) returns 1.0; 0.0 (fully wet) returns WET_SAND["darkening_min"].
    Curved so the eye notices the change in the first 20% of wetness.
    """
    w = min(1.0, max(0.0, float(wetness)))
    floor = WET_SAND["darkening_min"]
    return 1.0 - (1.0 - floor) * (1.0 - w) ** 1.6


def wetness_roughness(wetness: float) -> float:
    """Roughness for a surface at `wetness` in [0, 1], lerped wet<->dry."""
    w = min(1.0, max(0.0, float(wetness)))
    return WET_SAND["roughness_dry"] * w + WET_SAND["roughness_wet"] * (1.0 - w)


def foam_amount(kind: str, *, strength: float = 1.0) -> float:
    """Coverage in [0, 1] for one foam generator rule.

    `kind` is one of `breaking_crest`, `shoreline_swash`, `obstacle_wake`.
    `strength` is the rule's own driver (wave height ratio, swash ratio, flow
    ratio) already normalised to [0, 1] by the caller. The result is clamped to
    the rule's `max_coverage` and to FOAM_RULES["max_coverage"].
    """
    rules = FOAM_RULES.get(kind)
    if rules is None:
        raise KeyError(
            f"unknown foam rule {kind!r}; "
            f"known: {sorted(k for k in FOAM_RULES if isinstance(FOAM_RULES[k], dict))}"
        )
    s = min(1.0, max(0.0, float(strength)))
    return min(FOAM_RULES["max_coverage"], rules["max_coverage"]) * s


def sea_state_glint(sea_state: int | str, illumination: str = "clear_sky") -> dict[str, float]:
    """Specular parameters for a sea state under an illumination state.

    `sea_state` is a Beaufort number 0-9 (int or numeric string).
    `illumination` is a key of LIGHTING.
    """
    try:
        key = int(sea_state)
    except (TypeError, ValueError) as exc:
        raise KeyError(f"sea_state must be a Beaufort number 0-9, got {sea_state!r}") from exc
    if key not in SUN_GLINT_BANDS:
        raise KeyError(f"sea_state must be 0-9, got {key}")
    if illumination not in LIGHTING:
        raise KeyError(f"unknown illumination {illumination!r}; known: {sorted(LIGHTING)}")
    band = dict(SUN_GLINT_BANDS[key])
    light = LIGHTING[illumination]
    roughness = min(SUN_GLINT_ROUGHNESS_MAX,
                    max(0.0, band["roughness"] + light["roughness_offset"]))
    band.pop("label", None)
    band.pop("beaufort_name", None)
    band["roughness"] = roughness
    band["specular_gain"] = light["specular_gain"]
    band["ambient_gain"] = light["ambient_gain"]
    band["slope_variance"] = band.pop("slope_variance")
    return band


def lighting_response(illumination: str, *, sea_state: int = 3,
                      rain: float = 0.0) -> dict[str, Any]:
    """Merged specular/ambient/wet response for one illumination state.

    `rain` in [0, 1] is the rain intensity and drives the rain-roughness override.
    """
    if illumination not in LIGHTING:
        raise KeyError(f"unknown illumination {illumination!r}; known: {sorted(LIGHTING)}")
    rain = min(1.0, max(0.0, float(rain)))
    glint = sea_state_glint(sea_state, illumination)
    light = dict(LIGHTING[illumination])
    light["rain_roughness"] = RAIN_RESPONSE["roughness_gain"] * rain
    light["roughness"] = glint["roughness"] + light["rain_roughness"]
    light["sea_state"] = int(sea_state)
    light["rain"] = rain
    return light


# ==== serialisable table =====================================================
def _param(value: Any, source: str, unit: str | None = None,
           artistic_override: str | None = None) -> dict[str, Any]:
    out: dict[str, Any] = {"value": value, "source": source, "verified": SOURCE_NOTES.get(source, {}).get("verified", False)}
    if unit is not None:
        out["unit"] = unit
    if artistic_override:
        out["artistic_override"] = artistic_override
    return out


def optics_table() -> dict[str, Any]:
    """Every optical parameter as a JSON-serialisable dict.

    Values are plain (numbers, strings, lists, tuples, dicts) so the result can
    be written straight into a shader definition. Each leaf carries `source`
    and `verified`; entries that are not measurements carry `artistic_override`.
    """
    return {
        "model": {
            "name": "stalthos-archipelago-water-v1",
            "kind": "presentation-tuned optical parameter set",
            "note": (
                "Absorption shape is measured (Pope & Fry 1997). Scattering scale, "
                "glint bands, wet-sand darkening, foam coverage and SSS tint are "
                "artistic. See docs/water-and-shorelines.md."
            ),
        },
        "wavelength_bands_nm": _param(WAVELENGTH_BANDS, "pope97", "nm",
                                       "ARTISTIC: three display bands, not a spectral model."),
        "pure_water_absorption_m": dict(
            _param({str(k): v for k, v in PURE_WATER_ABSORPTION_M.items()}, "pope97", "m^-1")),
        "clearest_natural_water_absorption_m": dict(
            _param({str(k): v for k, v in CLEAREST_NATURAL_WATER_ABSORPTION_M.items()}, "smith81", "m^-1")),
        "water_classes": {
            name: {
                "family": spec["family"],
                "jerlov_type": spec["jerlov_type"],
                "label": spec["label"],
                "kd_450_m": _param(spec["kd_450_m"], "jerlov76", "m^-1", spec["note"]),
                "scale": _param(spec["scale"], "pope97", None, spec["note"]),
                "scattering_albedo": _param(
                    spec["scattering_albedo"], "mobley94", None, "ARTISTIC."),
                "verified": spec["verified"],
            }
            for name, spec in WATER_CLASSES.items()
        },
        "jerlov_water_classes": sorted(JERLOV_WATER_CLASSES),
        "refractive_index": {
            "ior_air": _param(IOR_AIR, "pope97", None,
                              "ARTISTIC: dispersion curve collapsed to one constant."),
            "ior_water": _param(IOR_WATER, "pope97", None,
                                "ARTISTIC: single constant, not a dispersion curve."),
            "ior_seawater": _param(IOR_SEAWATER, "jerlov76", None,
                                  "ARTISTIC: standard table value, salinity dispersion ignored."),
            "seawater_salinity_psu": _param(SEAWATER_SALINITY_PSU, "jerlov76", "PSU",
                                           "ARTISTIC world-building choice."),
        },
        "fresnel": {
            "f0_air_water": _param(F0_AIR_WATER, "pope97", None,
                                  "Exact from the refractive indices; not tuned."),
            "critical_angle_water_air_rad": _param(
                CRITICAL_ANGLE_WATER_AIR_RAD, "pope97", "rad",
                "Exact from Snell's law; not tuned."),
        },
        "scattering": {
            "wavelength_exponent": _param(
                SCATTERING_WAVELENGTH_EXPONENT, "mobley94", None,
                "ARTISTIC: sits between Rayleigh-like -4.3 and coastal ~-1."),
            "pure_water_scattering_m": _param(
                PURE_WATER_SCATTERING_M, "mobley94", "m^-1",
                "ARTISTIC: calibrated against the measured absorption, not measured."),
            "albedo": {
                name: _param(spec["scattering_albedo"], "mobley94", None,
                             "ARTISTIC: inside Mobley's stated qualitative ranges.")
                for name, spec in WATER_CLASSES.items()
            },
            "turbidity_units": list(TURBIDITY_UNITS),
            "turbidity_to_scattering": {
                unit: _param(v, "mobley94", "m^-1 per unit",
                             "ARTISTIC: presentation scale, not a calibration.")
                for unit, v in TURBIDITY_TO_SCATTERING.items()
            },
        },
        "sun_glint": {
            str(k): {
                "label": SUN_GLINT_LABELS[k],
                "beaufort_name": band["beaufort_name"],
                "wind_ms": _param(band["wind_ms"], "mobley94", "m/s",
                                  "ARTISTIC: representative, from the Cox-Munk U10 range."),
                "slope_variance": _param(band["slope_variance"], "mobley94", None,
                                         "ARTISTIC fit to the Cox-Munk wind-slope law."),
                "roughness": _param(band["roughness"], "mobley94", None,
                                    "ARTISTIC: presentation roughness band."),
                "normal_scale": _param(band["normal_scale"], "mobley94", None,
                                       "ARTISTIC: presentation normal strength."),
            }
            for k, band in SUN_GLINT_BANDS.items()
        },
        "sun_glint_roughness_max": _param(
            SUN_GLINT_ROUGHNESS_MAX, "mobley94", None,
            "ARTISTIC: above this a Blinn lobe stops reading as water."),
        "depth_ramp": {
            "max_depth_m": _param(DEPTH_RAMP["max_depth_m"], "pope97", "m",
                                  "ARTISTIC: presentation depth ceiling."),
            "stops_m": _param(DEPTH_RAMP["stops_m"], "pope97", "m",
                              "ARTISTIC: placed at readable milestones, not uniformly."),
            "opaque_depth_m": _param(DEPTH_RAMP["opaque_depth_m"], "pope97", "m",
                                     "ARTISTIC: presentation depth ceiling."),
            "red_gone_depth_m": _param(DEPTH_RAMP["red_gone_depth_m"], "pope97", "m",
                                       "Derived: -ln(0.1)/a(615 nm) with measured a."),
        },
        "wet_sand": {
            "darkening_min": _param(WET_SAND["darkening_min"], "mobley94", None,
                                    "ARTISTIC: mid-range of a literature range I could not re-measure."),
            "roughness_wet": _param(WET_SAND["roughness_wet"], "mobley94", None, "ARTISTIC."),
            "roughness_dry": _param(WET_SAND["roughness_dry"], "mobley94", None, "ARTISTIC."),
            "film_specular": _param(WET_SAND["film_specular"], "mobley94", None, "ARTISTIC."),
            "capillary_fringe_m": _param(WET_SAND["capillary_fringe_m"], "mobley94", "m",
                                         "ARTISTIC: chosen at Hero Beast body scale."),
            "damp_darkening": _param(WET_SAND["damp_darkening"], "mobley94", None, "ARTISTIC."),
        },
        "drying": {
            "time_constant_s": _param(DRYING["time_constant_s"], "mobley94", "s",
                                      "ARTISTIC: presentation time constant."),
            "curve_shape": _param(DRYING["curve_shape"], "mobley94", None, "ARTISTIC."),
            "residual_darkening": _param(DRYING["residual_darkening"], "mobley94", None, "ARTISTIC."),
        },
        "foam": {
            "max_coverage": _param(FOAM_RULES["max_coverage"], "mobley94", None,
                                   "ARTISTIC: hard clamp on coverage."),
            "bubble_scale_mm": _param(FOAM_RULES["bubble_scale_mm"], "mobley94", "mm", "ARTISTIC."),
            "roughness": _param(FOAM_RULES["roughness"], "mobley94", None, "ARTISTIC."),
            "rules": {
                kind: {
                    field: _param(val, "mobley94", None, "ARTISTIC: bounded display parameter.")
                    if not isinstance(val, dict) else val
                    for field, val in rule.items()
                }
                for kind, rule in FOAM_RULES.items() if isinstance(rule, dict)
            },
        },
        "shallow_sss": {
            "tint_rgb": _param(list(SHALLOW_SSS["tint_rgb"]), "mobley94", None,
                               "ARTISTIC: presentation tint."),
            "min_depth_m": _param(SHALLOW_SSS["min_depth_m"], "mobley94", "m", "ARTISTIC."),
            "max_depth_m": _param(SHALLOW_SSS["max_depth_m"], "mobley94", "m", "ARTISTIC."),
            "strength": _param(SHALLOW_SSS["strength"], "mobley94", None, "ARTISTIC."),
            "bed_albedo_boost": _param(SHALLOW_SSS["bed_albedo_boost"], "mobley94", None, "ARTISTIC."),
        },
        "lighting": {
            state: {
                key: _param(val, "mobley94", None, "ARTISTIC: presentation response knob.")
                for key, val in params.items()
            }
            for state, params in LIGHTING.items()
        },
        "rain_response": {
            key: _param(val, "mobley94", None, "ARTISTIC: presentation response knob.")
            for key, val in RAIN_RESPONSE.items()
        },
        "source_notes": SOURCE_NOTES,
    }
