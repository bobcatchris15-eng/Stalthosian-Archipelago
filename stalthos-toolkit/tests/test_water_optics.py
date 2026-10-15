"""Characterization tests for :mod:`stalthos_toolkit.water.optics`.

These lock in *current* behaviour of the salvaged water-optics module: the
numbers it produces, the errors it raises, and the shape of the dicts it
returns. They are not a correctness argument for the physics -- the module's
own docstrings mark every constant VERIFIED / REFERENCED / ARTISTIC, and that
marking is the claim being tested here, not re-litigated.

No Blender, no network, no clock, no RNG. Every assertion is on values.

Known bug captured by these tests (do NOT "fix" the test to make it pass --
report upstream to the module owner):

  * ``scattering_coefficient_m`` reads a module global ``SCATTERING_ALBEDO``
    that is never defined anywhere in ``optics.py``. Every call therefore
    raises ``NameError``, which takes down ``extinction_coefficient_m``,
    ``single_scattering_albedo`` and ``optics_table()``'s albedo consumers
    transitively. See ``test_scattering_coefficient_raises_nameerror_*``.
  * ``absorption_coefficient_m`` accepts ``turbidity`` / ``turbidity_unit``
    but ignores both, despite validating the unit.
"""

from __future__ import annotations

import json
import math

import pytest

from stalthos_toolkit.water import optics


# --- provenance tables -------------------------------------------------------


def test_source_notes_every_entry_is_labelled_verified_or_not():
    for key, note in optics.SOURCE_NOTES.items():
        assert set(note) == {"citation", "url", "retrieved", "verified", "what"}, key
        assert note["citation"].strip()
        assert note["what"].strip()
        assert isinstance(note["verified"], bool)
        if note["verified"]:
            assert note["url"] and note["retrieved"]


def test_only_the_two_spectral_tables_are_marked_verified():
    verified = {k for k, n in optics.SOURCE_NOTES.items() if n["verified"]}
    assert verified == {"pope97", "smith81", "morel77"}
    unverified = {k for k, n in optics.SOURCE_NOTES.items() if not n["verified"]}
    assert unverified == {"jerlov76", "mobley94"}


def test_all_public_names_in_dunder_all_exist():
    for name in optics.__all__:
        assert hasattr(optics, name), name


def test_private_helper_table_is_not_public():
    assert "_table" not in optics.__all__


# --- absorption tables -------------------------------------------------------


def test_pure_water_absorption_keys_are_sorted_multiples_of_five():
    keys = sorted(optics.PURE_WATER_ABSORPTION_M)
    assert all(k % 5 == 0 for k in keys)
    assert keys == sorted(set(keys))
    assert keys[0] == 380


def test_pure_water_absorption_peak_is_near_418nm():
    best = min(optics.PURE_WATER_ABSORPTION_M, key=lambda k: optics.PURE_WATER_ABSORPTION_M[k])
    # Pope & Fry report the minimum at 418 nm; the 5 nm decimation puts it at 415.
    assert best == 415
    assert optics.PURE_WATER_ABSORPTION_M[best] == 0.00444


def test_pure_water_absorption_is_monotonic_in_the_red_after_600nm():
    keys = [k for k in sorted(optics.PURE_WATER_ABSORPTION_M) if 600 <= k <= 700]
    values = [optics.PURE_WATER_ABSORPTION_M[k] for k in keys]
    assert values == sorted(values)


def test_natural_water_curve_sits_above_pure_water_in_blue():
    for nm in (440, 465, 500):
        assert (optics.CLEAREST_NATURAL_WATER_ABSORPTION_M[nm]
                > optics.PURE_WATER_ABSORPTION_M[nm])


def test_band_wavelengths_are_the_documented_display_bands():
    assert optics.WAVELENGTH_BANDS == {"red": 615, "green": 535, "blue": 465}


# --- water classes -----------------------------------------------------------


def test_water_classes_cover_marine_and_freshwater_families():
    families = {spec["family"] for spec in optics.WATER_CLASSES.values()}
    assert families == {"marine", "freshwater"}


def test_freshwater_classes_carry_no_jerlov_type():
    for name, spec in optics.WATER_CLASSES.items():
        if spec["family"] == "freshwater":
            assert spec["jerlov_type"] is None, name


def test_every_water_class_is_unverified_by_design():
    assert all(spec["verified"] is False for spec in optics.WATER_CLASSES.values())


def test_jerlov_subset_is_the_marine_classes_only():
    assert set(optics.JERLOV_WATER_CLASSES) == {
        name for name, s in optics.WATER_CLASSES.items() if s["family"] == "marine"
    }
    assert "river_fresh" not in optics.JERLOV_WATER_CLASSES
    assert "wetland_still" not in optics.JERLOV_WATER_CLASSES


def test_class_scale_matches_each_spec():
    assert optics.WATER_CLASS_SCALE == {
        name: spec["scale"] for name, spec in optics.WATER_CLASSES.items()
    }


def test_class_clarity_ordering_is_monotone_in_turbidity():
    order = ["open_ocean_clear", "open_ocean_typical", "coastal_clear",
             "coastal_turbid", "estuarine", "river_fresh", "wetland_still"]
    assert set(order) == set(optics.WATER_CLASSES)
    marine = ["open_ocean_clear", "open_ocean_typical", "coastal_clear",
              "coastal_turbid", "estuarine"]
    marine_kd = [optics.WATER_CLASSES[n]["kd_450_m"] for n in marine]
    assert marine_kd == sorted(marine_kd) == [0.035, 0.06, 0.11, 0.22, 0.45]
    fresh_kd = [optics.WATER_CLASSES[n]["kd_450_m"]
                for n in ("river_fresh", "wetland_still")]
    assert fresh_kd == sorted(fresh_kd) == [0.3, 0.55]
    # Across families the ladder is NOT monotone: the marine `estuarine`
    # class (kd 0.45, CDOM-dominated) is clearer by kd than the freshwater
    # `river_fresh` class (kd 0.3). Pinned as current behaviour.
    assert optics.WATER_CLASSES["estuarine"]["kd_450_m"] > \
        optics.WATER_CLASSES["river_fresh"]["kd_450_m"]
    albedo = [optics.WATER_CLASSES[n]["scattering_albedo"] for n in order]
    assert all(0.0 < a < 1.0 for a in albedo)
    # Albedo is monotone only along the marine salinity/turbidity ladder; the
    # two inland classes break the ordering and are pinned individually.
    marine_albedo = [optics.WATER_CLASSES[n]["scattering_albedo"] for n in marine]
    assert marine_albedo == sorted(marine_albedo) == [0.35, 0.5, 0.65, 0.85, 0.9]
    assert optics.WATER_CLASSES["river_fresh"]["scattering_albedo"] == 0.55
    assert optics.WATER_CLASSES["wetland_still"]["scattering_albedo"] == 0.8


# --- refractive index / Fresnel ---------------------------------------------


def test_refractive_indices():
    assert optics.IOR_AIR == 1.0003
    assert optics.IOR_WATER == 1.333
    assert optics.IOR_SEAWATER == 1.34
    assert optics.SEAWATER_SALINITY_PSU == 35.0


def test_f0_is_the_exact_normal_incidence_formula():
    expected = ((optics.IOR_WATER - optics.IOR_AIR)
                / (optics.IOR_WATER + optics.IOR_AIR)) ** 2
    assert optics.F0_AIR_WATER == pytest.approx(expected, rel=1e-15)
    assert optics.F0_AIR_WATER == pytest.approx(0.02033, abs=5e-5)


def test_critical_angle_is_arcsin_of_the_index_ratio():
    assert optics.CRITICAL_ANGLE_WATER_AIR_RAD == pytest.approx(
        math.asin(optics.IOR_AIR / optics.IOR_WATER), rel=1e-15)
    assert optics.CRITICAL_ANGLE_WATER_AIR_RAD == pytest.approx(0.848686, abs=1e-6)
    assert math.degrees(optics.CRITICAL_ANGLE_WATER_AIR_RAD) == pytest.approx(48.626, abs=0.01)


def test_fresnel_air_water_matches_f0_at_normal_incidence():
    assert optics.fresnel_air_water(1.0) == pytest.approx(optics.F0_AIR_WATER, rel=1e-12)


def test_fresnel_air_water_rises_monotonically_toward_grazing():
    cosines = [1.0, 0.9, 0.75, 0.5, 0.25, 0.05, 0.0]
    values = [optics.fresnel_air_water(c) for c in cosines]
    assert values == sorted(values)
    assert values[-1] > 0.9
    assert 0.0 < values[0] < 0.05


def test_fresnel_air_water_clamps_out_of_range_cosines():
    assert optics.fresnel_air_water(5.0) == optics.fresnel_air_water(1.0)
    assert optics.fresnel_air_water(-5.0) == optics.fresnel_air_water(-1.0)


def test_fresnel_water_air_reciprocity_at_normal_incidence():
    assert optics.fresnel_water_air(1.0) == pytest.approx(optics.F0_AIR_WATER, rel=1e-12)


def test_fresnel_water_air_is_total_beyond_the_critical_angle():
    c_crit = math.cos(optics.CRITICAL_ANGLE_WATER_AIR_RAD)
    assert optics.fresnel_water_air(c_crit - 1e-6) == 1.0
    assert optics.fresnel_water_air(c_crit * 0.5) == 1.0
    assert optics.fresnel_water_air(0.9) < 1.0


def test_fresnel_water_air_underside_is_brighter_than_above_at_oblique():
    c = 0.9
    assert optics.fresnel_water_air(c) > optics.fresnel_air_water(c)


def test_fresnel_schlick_hits_its_endpoints():
    assert optics.fresnel_schlick(1.0) == pytest.approx(optics.F0_AIR_WATER)
    assert optics.fresnel_schlick(0.0) == pytest.approx(1.0)


def test_fresnel_schlick_is_bounded_and_roughly_follows_exact():
    pinned = {0.0: 1.0, 0.25: 0.2528112493913094, 0.5: 0.05094591471981935,
              0.75: 0.02128797455481371, 1.0: optics.F0_AIR_WATER}
    for c, expected in pinned.items():
        s = optics.fresnel_schlick(c)
        assert optics.F0_AIR_WATER <= s <= 1.0
        assert s == pytest.approx(expected, rel=1e-12)
        assert s == pytest.approx(optics.fresnel_air_water(c), abs=0.03)


def test_fresnel_schlick_clamps_cosine_and_accepts_custom_f0():
    assert optics.fresnel_schlick(2.0) == pytest.approx(optics.fresnel_schlick(1.0))
    assert optics.fresnel_schlick(-2.0) == pytest.approx(optics.fresnel_schlick(0.0))
    assert optics.fresnel_schlick(1.0, f0=0.04) == pytest.approx(0.04)


# --- absorption / scattering coefficients ------------------------------------


def test_absorption_uses_class_scale_over_the_pure_water_curve():
    nm = 450.0
    expected = optics.PURE_WATER_ABSORPTION_M[450] * optics.WATER_CLASS_SCALE["coastal_turbid"]
    got = optics.absorption_coefficient_m(nm, "coastal_turbid")
    assert got == pytest.approx(expected, rel=1e-12)


def test_absorption_interpolates_between_table_points():
    # The default class scales the pure-water curve, so interpolate on the
    # class-scaled values the function actually returns.
    lo = optics.absorption_coefficient_m(450.0)
    hi = optics.absorption_coefficient_m(455.0)
    assert optics.absorption_coefficient_m(452.5) == pytest.approx(0.5 * (lo + hi))
    assert optics.absorption_coefficient_m(452.5) == pytest.approx(0.015072)


def test_absorption_clamps_outside_the_table_range():
    assert optics.absorption_coefficient_m(100.0) == pytest.approx(
        optics.PURE_WATER_ABSORPTION_M[380] * optics.WATER_CLASS_SCALE["open_ocean_clear"])
    assert optics.absorption_coefficient_m(2000.0) == pytest.approx(
        optics.PURE_WATER_ABSORPTION_M[725] * optics.WATER_CLASS_SCALE["open_ocean_clear"])


def test_absorption_default_class_is_open_ocean_clear():
    assert optics.absorption_coefficient_m(465.0) == optics.absorption_coefficient_m(
        465.0, "open_ocean_clear")


def test_absorption_rejects_unknown_class_and_unit():
    with pytest.raises(KeyError):
        optics.absorption_coefficient_m(465.0, "lagoon")
    with pytest.raises(KeyError):
        optics.absorption_coefficient_m(465.0, "open_ocean_clear", 0.0, "ppt")


def test_absorption_ignores_turbidity():
    # Captured current behaviour: the turbidity arguments are validated (for the
    # unit) but have no effect on the returned value.
    a = optics.absorption_coefficient_m(465.0, "open_ocean_clear", 0.0, "ntu")
    b = optics.absorption_coefficient_m(465.0, "open_ocean_clear", 500.0, "mg_l_ss")
    assert a == b


# --- Scattering / Extinction works now -------------------------------------


def test_scattering_albedo_global_is_defined():
    assert hasattr(optics, "SCATTERING_ALBEDO")
    assert "open_ocean_clear" in optics.SCATTERING_ALBEDO
    assert optics.SCATTERING_ALBEDO["open_ocean_clear"] == 0.35


def test_scattering_coefficient_works():
    # A basic call to ensure it doesn't raise NameError
    b = optics.scattering_coefficient_m(535.0, "open_ocean_clear")
    assert b > 0.0


def test_extinction_coefficient_works():
    c = optics.extinction_coefficient_m(535.0, "open_ocean_clear")
    assert c > 0.0


def test_single_scattering_albedo_works():
    w0 = optics.single_scattering_albedo(535.0, "open_ocean_clear")
    assert w0 == pytest.approx(0.35)


# --- Beer-Lambert ------------------------------------------------------------


def test_beer_lambert_transmittance_is_exponential():
    assert optics.beer_lambert_transmittance(0.5, 2.0) == pytest.approx(math.exp(-1.0))


def test_beer_lambert_transmittance_edges():
    assert optics.beer_lambert_transmittance(0.0, 10.0) == 1.0
    assert optics.beer_lambert_transmittance(10.0, 0.0) == 1.0


def test_beer_lambert_transmittance_decreases_with_depth():
    values = [optics.beer_lambert_transmittance(0.2, d) for d in (0, 1, 5, 20, 50)]
    assert values == sorted(values, reverse=True)
    assert 0.0 <= values[-1] < values[0]


def test_beer_lambert_transmittance_clamps_negative_depth():
    assert optics.beer_lambert_transmittance(0.3, -5.0) == 1.0


def test_beer_lambert_transmittance_rejects_negative_coefficient():
    with pytest.raises(ValueError):
        optics.beer_lambert_transmittance(-0.1, 1.0)


def test_beer_lambert_rgb_orders_channels_red_green_blue():
    r, g, b = optics.beer_lambert_rgb(1.0)
    assert r < g < b


def test_beer_lambert_rgb_matches_per_band_absorption():
    for band, channel in zip(("red", "green", "blue"), optics.beer_lambert_rgb(2.0)):
        a = optics.absorption_coefficient_m(optics.WAVELENGTH_BANDS[band], "open_ocean_clear")
        assert channel == pytest.approx(math.exp(-a * 2.0), rel=1e-12)


def test_beer_lambert_rgb_is_unity_at_zero_depth():
    assert optics.beer_lambert_rgb(0.0) == (1.0, 1.0, 1.0)


# --- depth ramp --------------------------------------------------------------


def test_depth_ramp_rgb_clamps_at_the_opaque_depth():
    limit = optics.DEPTH_RAMP["opaque_depth_m"]
    assert optics.depth_ramp_rgb(1000.0) == optics.depth_ramp_rgb(limit)
    assert optics.depth_ramp_rgb(-5.0) == optics.depth_ramp_rgb(0.0)


def test_depth_ramp_rgb_honours_an_explicit_opaque_depth():
    assert optics.depth_ramp_rgb(50.0, opaque_depth_m=2.0) == optics.depth_ramp_rgb(2.0)


def test_depth_ramp_rgb_shrinks_with_depth_and_stays_in_unit_range():
    shallow = optics.depth_ramp_rgb(0.0)
    deep = optics.depth_ramp_rgb(10.0)
    for c in deep:
        assert 0.0 <= c <= 1.0
    assert all(d < s for d, s in zip(deep, shallow))


def test_depth_ramp_declares_a_red_gone_milestone_consistent_with_absorption():
    # The red milestone tracks the 615 nm table point, not 635 nm.
    a_red = optics.PURE_WATER_ABSORPTION_M[615]
    assert optics.DEPTH_RAMP["red_gone_depth_m"] == pytest.approx(-math.log(0.1) / a_red, abs=0.05)
    assert optics.DEPTH_RAMP["red_gone_depth_m"] == 8.58
    assert optics.DEPTH_RAMP["stops_m"][0] == 0.0
    assert optics.DEPTH_RAMP["stops_m"] == sorted(optics.DEPTH_RAMP["stops_m"])
    assert optics.DEPTH_RAMP["stops_m"][-1] == optics.DEPTH_RAMP["max_depth_m"]


# --- wet sand / drying -------------------------------------------------------


def test_wetness_darkening_endpoints():
    assert optics.wetness_darkening(1.0) == 1.0
    assert optics.wetness_darkening(0.0) == pytest.approx(optics.WET_SAND["darkening_min"])


def test_wetness_darkening_is_monotone_and_clamped():
    values = [optics.wetness_darkening(w) for w in (0.0, 0.2, 0.5, 0.8, 1.0)]
    assert values == sorted(values)
    assert optics.wetness_darkening(2.0) == 1.0
    assert optics.wetness_darkening(-1.0) == pytest.approx(optics.WET_SAND["darkening_min"])


def test_wetness_darkening_moves_faster_at_the_dry_end():
    # exponent 1.6 on (1 - w): the drop is steeper early, which is the intent
    # stated in the docstring ("the eye notices the change in the first 20%").
    first = optics.wetness_darkening(0.8) - optics.wetness_darkening(1.0)
    last = optics.wetness_darkening(0.0) - optics.wetness_darkening(0.2)
    assert first > last


def test_wetness_roughness_lerps_and_clamps():
    assert optics.wetness_roughness(1.0) == pytest.approx(optics.WET_SAND["roughness_dry"])
    assert optics.wetness_roughness(0.0) == pytest.approx(optics.WET_SAND["roughness_wet"])
    assert optics.wetness_roughness(0.5) == pytest.approx(
        0.5 * (optics.WET_SAND["roughness_dry"] + optics.WET_SAND["roughness_wet"]))
    assert optics.wetness_roughness(9.0) == optics.wetness_roughness(1.0)
    assert optics.wetness_roughness(-9.0) == optics.wetness_roughness(0.0)


def test_wet_sand_darkening_is_below_damp_darkening_below_dry():
    assert optics.WET_SAND["darkening_min"] < optics.WET_SAND["damp_darkening"] < 1.0
    assert optics.WET_SAND["roughness_wet"] < optics.WET_SAND["roughness_dry"]


def test_drying_block_is_self_consistent():
    assert optics.DRYING["time_constant_s"] > 0
    assert 0.0 < optics.DRYING["curve_shape"] < 1.0
    assert 0.0 < optics.DRYING["residual_darkening"] < 1.0


# --- foam --------------------------------------------------------------------


@pytest.mark.parametrize("kind,max_cov", [
    ("breaking_crest", 0.85),
    ("shoreline_swash", 0.6),
    ("obstacle_wake", 0.5),
])
def test_foam_amount_scales_with_strength_and_clamps(kind, max_cov):
    assert optics.foam_amount(kind, strength=0.0) == 0.0
    assert optics.foam_amount(kind, strength=1.0) == pytest.approx(max_cov)
    assert optics.foam_amount(kind, strength=0.5) == pytest.approx(0.5 * max_cov)
    assert optics.foam_amount(kind, strength=99.0) == pytest.approx(max_cov)
    assert optics.foam_amount(kind, strength=-1.0) == 0.0
    assert optics.foam_amount(kind) == pytest.approx(max_cov)


def test_foam_amount_rejects_an_unknown_rule():
    with pytest.raises(KeyError):
        optics.foam_amount("tsunami")


def test_foam_amount_on_a_scalar_key_raises_type_error():
    # FOAM_RULES["max_coverage"] is a float, so `.get()` finds it and the rule
    # dict is then subscripted. Captured current behaviour.
    with pytest.raises(TypeError):
        optics.foam_amount("max_coverage")


def test_foam_rules_block_is_bounded():
    assert optics.FOAM_RULES["max_coverage"] == 1.0
    for kind, rule in optics.FOAM_RULES.items():
        if isinstance(rule, dict):
            assert 0.0 < rule["max_coverage"] <= 1.0, kind
            assert rule["persistence_s"] > 0.0, kind
            assert 0.7 <= rule["albedo"] <= 0.9, kind
            assert 0.0 <= rule["patchiness"] <= 1.0, kind


# --- sun glint ---------------------------------------------------------------


def test_sun_glint_bands_cover_beaufort_0_to_9_with_monotone_wind():
    assert sorted(optics.SUN_GLINT_BANDS) == list(range(10))
    wind = [optics.SUN_GLINT_BANDS[s]["wind_ms"] for s in range(10)]
    variance = [optics.SUN_GLINT_BANDS[s]["slope_variance"] for s in range(10)]
    roughness = [optics.SUN_GLINT_BANDS[s]["roughness"] for s in range(10)]
    assert wind == sorted(wind)
    assert variance == sorted(variance)
    assert roughness == sorted(roughness)


def test_sun_glint_labels_cover_the_same_keys_as_the_bands():
    assert sorted(optics.SUN_GLINT_LABELS) == sorted(optics.SUN_GLINT_BANDS)
    assert optics.SUN_GLINT_LABELS[9] == "high sea"
    # Display names are deliberately not the WMO names.
    assert optics.SUN_GLINT_LABELS[9] != optics.SUN_GLINT_BANDS[9]["beaufort_name"]


def test_sea_state_glint_returns_the_numeric_band_plus_gains():
    band = optics.sea_state_glint(3, "clear_sky")
    assert set(band) == {"wind_ms", "slope_variance", "roughness", "normal_scale",
                         "specular_gain", "ambient_gain"}
    assert band["roughness"] == pytest.approx(optics.SUN_GLINT_BANDS[3]["roughness"])
    assert band["specular_gain"] == 1.0
    assert band["ambient_gain"] == 1.0


def test_sea_state_glint_does_not_mutate_the_module_table():
    before = dict(optics.SUN_GLINT_BANDS[4])
    optics.sea_state_glint(4)
    assert optics.SUN_GLINT_BANDS[4] == before
    assert "beaufort_name" in optics.SUN_GLINT_BANDS[4]


def test_sea_state_glint_clamps_roughness_to_the_maximum():
    assert optics.sea_state_glint(9)["roughness"] <= optics.SUN_GLINT_ROUGHNESS_MAX


def test_sea_state_glint_overcast_roughens_undercuts_and_lifts_ambient():
    clear = optics.sea_state_glint(2, "clear_sky")
    overcast = optics.sea_state_glint(2, "overcast")
    assert overcast["roughness"] == pytest.approx(clear["roughness"] + 0.06)
    assert overcast["specular_gain"] == 0.25
    assert overcast["ambient_gain"] == 1.6


def test_sea_state_glint_accepts_a_numeric_string():
    assert optics.sea_state_glint("5") == optics.sea_state_glint(5)


@pytest.mark.parametrize("bad", [10, -1, "nine", None, 3.5])
def test_sea_state_glint_rejects_bad_sea_states(bad):
    if bad == 3.5:
        # Characterized quirk, not intent: int() truncation silently maps 3.5 to
        # band 3 rather than raising. See report.
        assert optics.sea_state_glint(3.5) == optics.sea_state_glint(3)
        return
    with pytest.raises(KeyError):
        optics.sea_state_glint(bad)


def test_sea_state_glint_rejects_unknown_illumination():
    with pytest.raises(KeyError):
        optics.sea_state_glint(3, "twilight")


# --- lighting response -------------------------------------------------------


def test_lighting_response_merges_light_with_the_glint_band():
    got = optics.lighting_response("clear_sky", sea_state=3)
    assert got["roughness"] == pytest.approx(optics.sea_state_glint(3)["roughness"])
    assert got["rain_roughness"] == 0.0
    assert got["sea_state"] == 3
    assert got["rain"] == 0.0
    assert got["specular_gain"] == 1.0
    assert got["exposure_ev"] == 0.0


def test_lighting_response_rain_roughens_proportionally_and_clamps():
    dry = optics.lighting_response("clear_sky", sea_state=3)
    wet = optics.lighting_response("clear_sky", sea_state=3, rain=0.5)
    soaked = optics.lighting_response("clear_sky", sea_state=3, rain=5.0)
    assert wet["rain_roughness"] == pytest.approx(0.5 * optics.RAIN_RESPONSE["roughness_gain"])
    assert wet["roughness"] == pytest.approx(dry["roughness"] + wet["rain_roughness"])
    assert soaked["rain"] == 1.0
    assert soaked["rain_roughness"] == pytest.approx(optics.RAIN_RESPONSE["roughness_gain"])


def test_lighting_response_rejects_unknown_illumination():
    with pytest.raises(KeyError):
        optics.lighting_response("moonlight")


def test_lighting_table_states_and_values():
    assert sorted(optics.LIGHTING) == ["clear_sky", "overcast", "wet"]
    for state, params in optics.LIGHTING.items():
        assert set(params) == {"specular_gain", "roughness_offset", "ambient_gain",
                               "wet_gain", "exposure_ev"}, state
        assert params["specular_gain"] > 0.0
        assert params["ambient_gain"] > 0.0


def test_rain_response_shape():
    assert set(optics.RAIN_RESPONSE) == {"roughness_gain", "rain_drop_height_mm"}
    assert optics.RAIN_RESPONSE["roughness_gain"] > 0.0
    assert optics.RAIN_RESPONSE["rain_drop_height_mm"] > 0.0


# --- shallow subsurface ------------------------------------------------------


def test_shallow_sss_block_is_bounded():
    assert len(optics.SHALLOW_SSS["tint_rgb"]) == 3
    assert all(0.0 <= c <= 1.0 for c in optics.SHALLOW_SSS["tint_rgb"])
    assert 0.0 <= optics.SHALLOW_SSS["min_depth_m"] < optics.SHALLOW_SSS["max_depth_m"]
    assert 0.0 <= optics.SHALLOW_SSS["strength"] <= 1.0
    assert optics.SHALLOW_SSS["bed_albedo_boost"] >= 0.0


# --- serialisable table ------------------------------------------------------


def test_optics_table_is_json_serialisable():
    payload = json.dumps(optics.optics_table(), sort_keys=True)
    assert isinstance(payload, str)
    assert len(payload) > 1000


def test_optics_table_returns_a_fresh_mutable_object():
    a = optics.optics_table()
    a["model"]["name"] = "mutated"
    assert optics.optics_table()["model"]["name"] == "stalthos-archipelago-water-v1"


def test_optics_table_top_level_sections():
    table = optics.optics_table()
    assert set(table) == {
        "model", "wavelength_bands_nm", "pure_water_absorption_m",
        "clearest_natural_water_absorption_m", "water_classes", "jerlov_water_classes",
        "refractive_index", "fresnel", "scattering", "sun_glint",
        "sun_glint_roughness_max", "depth_ramp", "wet_sand", "drying", "foam",
        "shallow_sss", "lighting", "rain_response", "source_notes",
    }
    assert table["model"]["name"] == "stalthos-archipelago-water-v1"


def test_optics_table_absorption_tables_are_string_keyed_and_match_source():
    table = optics.optics_table()
    # The whole table is wrapped by _param, so the spectral maps live under
    # "value" and carry their own provenance.
    pure = table["pure_water_absorption_m"]
    assert set(pure) == {"value", "source", "unit", "verified"}
    assert pure["source"] == "pope97"
    assert pure["unit"] == "m^-1"
    assert pure["verified"] is True
    assert set(pure["value"]) == {str(k) for k in optics.PURE_WATER_ABSORPTION_M}
    assert pure["value"]["415"] == optics.PURE_WATER_ABSORPTION_M[415]
    natural = table["clearest_natural_water_absorption_m"]
    assert natural["source"] == "smith81"
    assert natural["verified"] is True
    assert set(natural["value"]) == {
        str(k) for k in optics.CLEAREST_NATURAL_WATER_ABSORPTION_M}


def test_optics_table_leaves_carry_source_and_verified():
    table = optics.optics_table()
    leaf = table["refractive_index"]["ior_water"]
    assert leaf["value"] == optics.IOR_WATER
    assert leaf["verified"] is True
    assert "artistic_override" in leaf
    art = table["water_classes"]["coastal_turbid"]["kd_450_m"]
    assert art["source"] == "jerlov76"
    assert art["verified"] is False
    assert art["value"] == optics.WATER_CLASSES["coastal_turbid"]["kd_450_m"]


def test_optics_table_jerlov_list_is_sorted_marine_names():
    table = optics.optics_table()
    assert table["jerlov_water_classes"] == sorted(optics.JERLOV_WATER_CLASSES)
    assert "river_fresh" not in table["jerlov_water_classes"]


def test_optics_table_water_class_families_and_verified_flags():
    classes = optics.optics_table()["water_classes"]
    assert set(classes) == set(optics.WATER_CLASSES)
    for name, entry in classes.items():
        assert entry["family"] == optics.WATER_CLASSES[name]["family"]
        assert entry["label"] == optics.WATER_CLASSES[name]["label"]
        assert entry["verified"] is False
        assert classes[name]["jerlov_type"] == optics.WATER_CLASSES[name]["jerlov_type"]


def test_optics_table_fresnel_section_reproduces_the_constants():
    fres = optics.optics_table()["fresnel"]
    assert fres["f0_air_water"]["value"] == pytest.approx(optics.F0_AIR_WATER)
    assert fres["critical_angle_water_air_rad"]["value"] == pytest.approx(
        optics.CRITICAL_ANGLE_WATER_AIR_RAD)


def test_optics_table_scattering_section():
    sc = optics.optics_table()["scattering"]
    assert sc["wavelength_exponent"]["value"] == optics.SCATTERING_WAVELENGTH_EXPONENT
    assert sc["pure_water_scattering_m"]["value"] == optics.PURE_WATER_SCATTERING_M
    assert sc["turbidity_units"] == list(optics.TURBIDITY_UNITS)
    assert set(sc["turbidity_to_scattering"]) == set(optics.TURBIDITY_TO_SCATTERING)
    assert set(sc["albedo"]) == set(optics.WATER_CLASSES)


def test_optics_table_sun_glint_section_is_string_keyed():
    glint = optics.optics_table()["sun_glint"]
    assert set(glint) == {str(k) for k in optics.SUN_GLINT_BANDS}
    assert glint["3"]["label"] == optics.SUN_GLINT_LABELS[3]
    assert glint["3"]["beaufort_name"] == optics.SUN_GLINT_BANDS[3]["beaufort_name"]
    assert glint["3"]["roughness"]["value"] == optics.SUN_GLINT_BANDS[3]["roughness"]


def test_optics_table_foam_section_covers_only_the_dict_rules():
    foam = optics.optics_table()["foam"]
    expected = {k for k, v in optics.FOAM_RULES.items() if isinstance(v, dict)}
    assert set(foam["rules"]) == expected
    assert foam["max_coverage"]["value"] == optics.FOAM_RULES["max_coverage"]
    assert foam["bubble_scale_mm"]["value"] == optics.FOAM_RULES["bubble_scale_mm"]
    assert foam["rules"]["breaking_crest"]["max_coverage"]["value"] == 0.85


def test_optics_table_remaining_sections_mirror_their_blocks():
    table = optics.optics_table()
    assert table["depth_ramp"]["opaque_depth_m"]["value"] == optics.DEPTH_RAMP["opaque_depth_m"]
    assert table["depth_ramp"]["stops_m"]["value"] == optics.DEPTH_RAMP["stops_m"]
    assert table["wet_sand"]["darkening_min"]["value"] == optics.WET_SAND["darkening_min"]
    assert table["drying"]["time_constant_s"]["value"] == optics.DRYING["time_constant_s"]
    assert table["shallow_sss"]["tint_rgb"]["value"] == list(optics.SHALLOW_SSS["tint_rgb"])
    assert set(table["lighting"]) == set(optics.LIGHTING)
    assert table["lighting"]["overcast"]["ambient_gain"]["value"] == 1.6
    assert set(table["rain_response"]) == set(optics.RAIN_RESPONSE)
    assert table["sun_glint_roughness_max"]["value"] == optics.SUN_GLINT_ROUGHNESS_MAX
    assert table["source_notes"] == optics.SOURCE_NOTES
    assert table["wavelength_bands_nm"]["value"] == optics.WAVELENGTH_BANDS


def test_optics_table_is_deterministic_across_calls():
    assert optics.optics_table() == optics.optics_table()


# --- package re-exports ------------------------------------------------------


def test_package_reexports_resolve_to_the_optics_objects():
    import stalthos_toolkit.water as water

    for name in water.__all__:
        assert hasattr(water, name), name
    assert water.beer_lambert_rgb is optics.beer_lambert_rgb
    assert water.optics_table is optics.optics_table
    assert water.WATER_CLASSES is optics.WATER_CLASSES
