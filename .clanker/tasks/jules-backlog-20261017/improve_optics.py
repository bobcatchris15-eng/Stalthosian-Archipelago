from pathlib import Path
w=Path('var/worktrees/jules-backlog-20261017'); p=w/'stalthos-toolkit/tests/test_water_optics.py'; t=p.read_text(); start=t.index('Known bug captured'); end=t.index('"""',start); t=t[:start]+'''The restored class albedo lookup is checked through the scattering,
extinction and transmission APIs. Absorption accepts turbidity but retains the
module's declared absorption-only model; this is characterized separately.
'''+t[end:]; t+='''

@pytest.mark.parametrize("water_class", sorted(optics.WATER_CLASSES))
def test_class_scattering_matches_declared_reference_albedo(water_class):
    wavelength = float(optics.WAVELENGTH_BANDS["green"])
    a = optics.absorption_coefficient_m(wavelength, water_class)
    b = optics.scattering_coefficient_m(wavelength, water_class)
    c = optics.extinction_coefficient_m(wavelength, water_class)
    assert c == pytest.approx(a + b)
    assert optics.single_scattering_albedo(wavelength, water_class) == pytest.approx(
        optics.WATER_CLASSES[water_class]["scattering_albedo"]
    )
    near = optics.beer_lambert_rgb(0.5, water_class)
    far = optics.beer_lambert_rgb(5.0, water_class)
    assert all(0.0 <= farther < nearer <= 1.0 for nearer, farther in zip(near, far))


@pytest.mark.parametrize("water_class", sorted(optics.WATER_CLASSES))
def test_added_turbidity_increases_extinction(water_class):
    assert optics.extinction_coefficient_m(535.0, water_class, turbidity=2.0) > (
        optics.extinction_coefficient_m(535.0, water_class)
    )


def test_scattering_rejects_unknown_class_and_unit():
    with pytest.raises(KeyError, match="unknown water class"):
        optics.scattering_coefficient_m(535.0, "missing")
    with pytest.raises(KeyError, match="unknown turbidity unit"):
        optics.scattering_coefficient_m(535.0, turbidity_unit="missing")
'''; p.write_text(t,encoding='utf-8')
