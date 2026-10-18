"""Malformed telemetry regression tests, requiring no engine or third party tools."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from verify_population_activation import check_interval, check_transition, load_json, snapshot


def state():
    return {'seed': 123, 'region': 1001, 'habitat_signature': 456,
            'actors': [dict(id=i, species=0, stage=2, activity=1, site=0, target=1,
                            x=0., y=0., z=0., heading=0., distance_m=1., age=4000.,
                            hunger=.2, thirst=.3, fatigue=.4, injury=0., is_resting=False)
                       for i in range(1, 9)]}


def interval():
    before = state()
    after = copy.deepcopy(before)
    for actor in after['actors']:
        actor.update(age=4060., hunger=.206, thirst=.312, fatigue=.41)
    return dict(before=before, after=after, physical_seconds=2., calendar_seconds=60.,
                reactivated=False, food_consumed=0., water_consumed=0.,
                resource_events=0, resource_sessions_unchanged=True)


class MalformedTelemetry(unittest.TestCase):
    def test_valid_elapsed_interval(self):
        self.assertEqual(check_interval(interval()), (2., 60.))

    def test_positive_elapsed_required(self):
        evidence=interval()
        evidence.update(physical_seconds=0, calendar_seconds=0, after=copy.deepcopy(evidence["before"]))
        with self.assertRaises(ValueError):
            check_interval(evidence)

    def test_transition_radius_must_match_activation(self):
        for active, x in ((False, 200.), (True, 100.)):
            transition=dict(physical_active=active, body_x=x, body_z=24., physical_seconds=2., snapshot=state())
            check_transition(transition, snapshot(state()), 192.)
            transition['physical_active']=not active
            with self.assertRaises(ValueError):
                check_transition(transition, snapshot(state()), 192.)

    def test_missing_actor_field(self):
        s = state()
        del s['actors'][0]['injury']
        with self.assertRaises(KeyError):
            snapshot(s)

    def test_booleans_and_numbers_do_not_coerce(self):
        for field, value in [('is_resting', None), ('is_resting', 'false'),
                             ('x', True), ('id', '1'), ('fatigue', float('nan'))]:
            with self.subTest(field=field, value=value):
                s = state()
                s['actors'][0][field] = value
                with self.assertRaises(ValueError):
                    snapshot(s)

    def test_duplicate_identity(self):
        s = state()
        s['actors'][1]['id'] = 1
        with self.assertRaises(ValueError):
            snapshot(s)

    def test_dormant_mutation(self):
        for field, value in [('x', .01), ('target', 0), ('activity', 2),
                             ('hunger', .2), ('fatigue', .4), ('injury', .01)]:
            with self.subTest(field=field):
                evidence = interval()
                evidence['after']['actors'][0][field] = value
                with self.assertRaises(ValueError):
                    check_interval(evidence)

    def test_no_dormant_contacts_or_session_changes(self):
        for field, value in [('resource_events', 1), ('food_consumed', .01),
                             ('water_consumed', .01), ('resource_sessions_unchanged', False),
                             ('resource_sessions_unchanged', 'true')]:
            with self.subTest(field=field):
                evidence = interval()
                evidence[field] = value
                with self.assertRaises(ValueError):
                    check_interval(evidence)

    def test_duplicate_json_key_and_nonfinite_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'bad.json'
            for contents in ('{"x":1,"x":2}', '{"x":NaN}', '[]'):
                path.write_text(contents, encoding='utf-8')
                with self.assertRaises(ValueError):
                    load_json(path)


if __name__ == '__main__':
    unittest.main()
