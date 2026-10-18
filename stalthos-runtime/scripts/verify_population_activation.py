#!/usr/bin/env python3
"""Verify four native population activation probes; stdlib only, fail closed."""
import argparse
import json
import math
from pathlib import Path
import sys


def require(ok, message):
    if not ok:
        raise ValueError(message)


def number(obj, key, minimum=0, integer=False):
    value = obj[key]
    require(type(value) is int if integer else type(value) in (int, float),
            f'{key}: expected {"integer" if integer else "number"}')
    require(math.isfinite(value) and value >= minimum, f'{key}: invalid finite range')
    return value


def boolean(obj, key):
    require(type(obj[key]) is bool, f'{key}: expected boolean')
    return obj[key]


def array(obj, key):
    require(type(obj[key]) is list, f'{key}: expected array')
    return obj[key]


def near(actual, expected, message):
    require(math.isclose(actual, expected, rel_tol=1e-10, abs_tol=1e-8), message)


def snapshot(s):
    require(type(s) is dict, 'snapshot must be object')
    for key in ('seed', 'region', 'habitat_signature'):
        number(s, key, integer=True)
    actors = array(s, 'actors')
    require(len(actors) == 8, 'snapshot must retain eight actors')
    ids = set()
    for a in actors:
        require(type(a) is dict, 'actor must be object')
        identity = number(a, 'id', 1, True)
        require(identity not in ids, 'duplicate actor identity')
        ids.add(identity)
        for key, maximum in (('species', 4), ('stage', 4), ('activity', 3)):
            require(number(a, key, integer=True) <= maximum, f'{key}: invalid enum')
        for key in ('site', 'target'):
            number(a, key, integer=True)
        for key in ('x', 'y', 'z', 'heading'):
            number(a, key, -math.inf)
        for key in ('distance_m', 'age'):
            number(a, key)
        for key in ('hunger', 'thirst', 'fatigue', 'injury'):
            require(number(a, key) <= 1, f'{key}: outside normalized range')
        boolean(a, 'is_resting')
    return ids


def load_json(path):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, f'{path}: duplicate key {key}')
            result[key] = value
        return result
    with path.open(encoding='utf-8-sig') as handle:
        value = json.load(handle, object_pairs_hook=pairs,
                          parse_constant=lambda value: (_ for _ in ()).throw(ValueError(f'nonfinite {value}')))
    require(type(value) is dict, f'{path}: expected object')
    return value


def check_interval(interval):
    before, after = interval['before'], interval['after']
    require(snapshot(before) == snapshot(after), 'interval identities changed')
    for key in ('seed', 'region', 'habitat_signature'):
        require(before[key] == after[key], f'interval {key} changed')
    physical = number(interval, 'physical_seconds')
    calendar = number(interval, 'calendar_seconds')
    require(physical > 0 and calendar > 0, 'dormant probe must have positive elapsed time')
    boolean(interval, 'reactivated')
    require(boolean(interval, 'resource_sessions_unchanged'), 'dormant resource sessions changed')
    for key in ('food_consumed', 'water_consumed', 'resource_events'):
        require(number(interval, key) == 0, f'dormant {key} must be zero')
    latter = {a['id']: a for a in after['actors']}
    for a in before['actors']:
        b = latter[a['id']]
        for key in ('species', 'x', 'y', 'z', 'heading', 'distance_m', 'site', 'target', 'injury', 'is_resting'):
            require(a[key] == b[key], f'dormant actor {a["id"]} changed {key}')
        near(b['age'], a['age'] + (calendar if a['stage'] != 4 else 0), 'dormant age mismatch')
        durations = ((1000, 2000, 10000, 3000), (1500, 3000, 15000, 4000),
                     (1200, 2500, 12000, 3500), (800, 1500, 8000, 2000),
                     (2000, 4000, 20000, 5000))[a['species']]
        threshold, expected_stage = 0, 0
        for duration in durations:
            threshold += duration
            if b['age'] >= threshold:
                expected_stage += 1
        require(b['stage'] == max(a['stage'], expected_stage), 'dormant lifecycle stage mismatch')
        if b['stage'] != 4:
            require(a['activity'] == b['activity'], 'living dormant activity changed')
            near(b['hunger'], min(1, a['hunger'] + calendar * .0001), 'dormant hunger mismatch')
            near(b['thirst'], min(1, a['thirst'] + calendar * .0002), 'dormant thirst mismatch')
            near(b['fatigue'], max(0, min(1, a['fatigue'] + physical * (-.05 if a['is_resting'] else .005))),
                 'dormant fatigue mismatch')
        else:
            require(b['activity'] == 0, 'dead actor activity must be Rest')
            # Death freezes needs; multi-frame intervals may contain pre-death evolution.
            if a['stage'] == 4:
                for key in ('hunger', 'thirst', 'fatigue'):
                    require(a[key] == b[key], 'dead dormant needs changed')
    return physical, calendar


def check_transition(t, ids, radius):
    boolean(t, 'physical_active')
    number(t, 'body_x', -math.inf)
    number(t, 'body_z', -math.inf)
    number(t, 'physical_seconds')
    require(t['physical_active'] == (math.hypot(t['body_x'], t['body_z']) <= radius), 'transition contradicts body radius')
    require(snapshot(t['snapshot']) == ids, 'transition identities changed')


def check_run(path, skins):
    p = load_json(path / 'world-population.json')
    r = load_json(path / 'runtime-receipt.json')
    require(number(p, 'schema_version', integer=True) == 2, 'requires population schema 2')
    require(number(r, 'resource_ledger_cap_exhaustions', integer=True) == 0, 'resource ledger cap exhausted')
    require(p['activation_driver'] == 'body', 'activation must follow body')
    require(number(p, 'activation_radius_m') == 192, 'unexpected activation radius')
    require(not boolean(p, 'camera_drives_simulation'), 'camera drives population')
    for key in ('population_persistence_supported', 'resources_use_shared_persisted_ledger'):
        require(boolean(p, key), f'{key} missing')
    for key in ('physical_active_initial', 'physical_active_final', 'population_restored', 'population_persisted'):
        boolean(p, key)
    ids = snapshot(p['snapshot_initial'])
    require(ids == snapshot(p['snapshot_final']), 'run identities changed')
    for key in ('actors_initial', 'actors_final'):
        views = array(p, key)
        require(len(views) == 8, 'actor view count mismatch')
        require({number(a, 'id', 1, True) for a in views} == ids, 'actor views identity mismatch')
        for a in views:
            boolean(a, 'alive')
            for field in ('x', 'y', 'z', 'heading'):
                number(a, field, -math.inf)
            for field in ('age', 'distance_m', 'hunger', 'thirst', 'fatigue'):
                number(a, field)
            number(a, 'activity', integer=True)
            require(type(a['species']) is str and a['species'], 'actor species missing')
    for key in ('simulation_frames', 'dormant_frames', 'pose_matrix_checks', 'skin_matrix_checks',
                'hidden_object_checks', 'visible_object_checks', 'resource_contact_events'):
        number(p, key, integer=True)
    require(p['pose_matrix_checks'] > 0, 'no pose matrix checks')
    if skins:
        require(p['skin_matrix_checks'] > 0, 'no skin matrix checks')
    for key in ('food_consumed', 'water_consumed', 'dormant_physical_seconds', 'dormant_calendar_seconds'):
        number(p, key)
    frames = array(p, 'activity_frames_rest_travel_browse_drink')
    require(len(frames) == 4, 'expected four activity frame counts')
    for count in frames:
        require(type(count) is int and count >= 0, 'invalid activity frame count')
    require(sum(frames) == p['simulation_frames'] * 8, 'activity frame counts inconsistent')
    totals = [0., 0.]
    for interval in array(p, 'dormant_intervals'):
        values = check_interval(interval)
        totals = [a + b for a, b in zip(totals, values)]
    near(totals[0], p['dormant_physical_seconds'], 'dormant physical duration inconsistent')
    near(totals[1], p['dormant_calendar_seconds'], 'dormant calendar duration inconsistent')
    for t in array(p, 'activation_transitions'):
        check_transition(t, ids, p['activation_radius_m'])
    return p, r


def verify(paths, skins=False):
    runs = [check_run(Path(p), skins) for p in paths]
    departure, dormant, returned, camera = [p for p, _ in runs]
    expected_ids = snapshot(departure['snapshot_initial'])
    for p, _ in runs:
        require(snapshot(p['snapshot_initial']) == expected_ids, 'cross-run identities changed')
    for previous, current in ((departure, dormant), (dormant, returned)):
        require(previous['snapshot_final'] == current['snapshot_initial'], 'full snapshot changed across save/load')
        require(current['population_restored'], 'population was not restored')
    require(departure['population_persisted'] and dormant['population_persisted'], 'continuation snapshot not saved')
    for p, initial, final in ((departure, True, False), (dormant, False, False), (returned, False, True), (camera, True, True)):
        require(p['physical_active_initial'] is initial and p['physical_active_final'] is final, 'incorrect activation endpoints')
    for p, active in ((departure, False), (returned, True)):
        require(len(p['activation_transitions']) == 1 and p['activation_transitions'][0]['physical_active'] is active,
                'expected exactly one body activation transition')
        require(len(p['dormant_intervals']) == 1 and p['dormant_frames'] > 0, 'missing dormant interval')
        interval = p['dormant_intervals'][0]
        require(interval['reactivated'] is active, 'interval reactivation mismatch')
        if active:
            require(interval['before'] == p['snapshot_initial'] and interval['after'] == p['activation_transitions'][0]['snapshot'], 'return interval boundaries differ')
        else:
            require(interval['before'] == p['activation_transitions'][0]['snapshot'] and interval['after'] == p['snapshot_final'], 'departure interval boundaries differ')
    require(not dormant['activation_transitions'] and dormant['simulation_frames'] == 0, 'inactive run simulated population')
    require(len(dormant['dormant_intervals']) == 1 and dormant['dormant_frames'] > 0, 'missing fully dormant interval')
    interval = dormant['dormant_intervals'][0]
    require(not interval['reactivated'] and interval['before'] == dormant['snapshot_initial'] and interval['after'] == dormant['snapshot_final'], 'fully dormant boundaries differ')
    for key in ('resource_contact_events', 'food_consumed', 'water_consumed'):
        require(dormant[key] == 0, f'inactive run {key} nonzero')
    for p in (departure, dormant):
        require(p['hidden_object_checks'] > 0, 'away run has no hidden object checks')
    require(returned['visible_object_checks'] > 0 and returned['simulation_frames'] > 0, 'return never resumed visible simulation')
    initial_distance = {a['id']: a['distance_m'] for a in returned['snapshot_initial']['actors']}
    require(any(a['distance_m'] > initial_distance[a['id']] for a in returned['snapshot_final']['actors']), 'return actors did not resume travel')
    require(not camera['activation_transitions'] and not camera['dormant_intervals'] and camera['dormant_frames'] == 0 and camera['simulation_frames'] > 0, 'camera changed population activation')
    cr = runs[3][1]
    require(boolean(cr, 'diagnostic_camera') and number(cr, 'diagnostic_camera_stage', integer=True) == 3, 'camera diagnostic incomplete')
    require(number(cr, 'detached_camera_frames', integer=True) > 0, 'no detached camera frames')
    require(number(cr, 'body_distance_while_camera_detached_m') == 0, 'body moved during camera detachment')
    return {'passed': True, 'actor_count': 8, 'exact_snapshot_continuation': True,
            'dormant_needs_verified': True, 'body_activation_verified': True,
            'camera_activation_independent': True, 'skins_required': skins}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('departure', 'dormant', 'return', 'camera'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--require-skins', action='store_true')
    args = parser.parse_args()
    try:
        result = verify([getattr(args, k) for k in ('departure', 'dormant', 'return', 'camera')], args.require_skins)
    except (OSError, ValueError, KeyError, TypeError, OverflowError) as exc:
        print(json.dumps({'passed': False, 'error': str(exc)}))
        return 1
    print(json.dumps(result, separators=(',', ':')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
