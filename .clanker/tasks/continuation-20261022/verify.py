from pathlib import Path
import hashlib, json, math, re

root = Path(__file__).resolve().parents[3]
task = Path(__file__).parent
read = lambda p: json.loads(p.read_text(encoding='utf-8-sig'))
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
executable = digest(root / 'var/build/stalthos-runtime/Release/Stalthos.exe')
metrics = {}
runs = {}
for label in ('body', 'cancel', 'camera', 'nest', 'normal', 'original'):
    p = Path(read(task / 'native-runs.json')[label])
    r = read(p / 'runtime-receipt.json')
    assert r['ok'] and r['support_failure_count'] == 0, (label, 'runtime/support')
    assert read(p / 'launch.json')['executable_sha256'] == executable
    metrics[label] = {'path': str(p), 'physical_seconds': r['physical_seconds'], 'support_failures': 0}
    runs[label] = p
    if label == 'original':
        assert not (p / 'nesting.json').exists() and not (p / 'carnivore-awareness.json').exists()
        continue
    assert r['rendered_ground_support_fallbacks'] == 0
    w = read(p / 'world-population.json')
    initial = {a['id']: a for a in w['snapshot_initial']['actors']}
    final = {a['id']: a for a in w['snapshot_final']['actors']}
    assert len(initial) == 12 and initial.keys() == final.keys()
    assert w['resource_sessions_initial'] == w['resource_sessions_final']
    for identity, a in final.items():
        assert abs(a['age'] - initial[identity]['age'] - r['calendar_seconds']) < 1e-6
    ledger = w['resource_ledger_final']
    for resource in ledger['resources']:
        total = sum(s['amount'] for s in ledger['sessions'] if s['resource_id'] == resource['id'])
        assert abs(total - resource['consumed_quantity']) < 1e-8
    metrics[label].update(actors=12, sessions=len(w['resource_sessions_final']), support_fallbacks=0)
    assert (p / 'nesting.json').exists() and (p / 'carnivore-awareness.json').exists(), 'final feature reports required'
    if label in ('body', 'cancel', 'camera'):
        j = read(p / 'carnivore-awareness.json'); s = j['samples']
        assert len(s) > 50 and j['watched_actor'] in initial
        assert len(j['captures']) >= 2 and all(c['saved'] and (p / c['filename']).is_file() for c in j['captures'])
        assert s[-1]['recall_requests'] == (0 if label == 'camera' else 1)
        if label == 'camera':
            assert w['physical_active_initial'] and w['physical_active_final'] and not w['activation_transitions']
            assert all(not v['cue_visible'] and not v['recall'] for v in s)
            assert all(v['body_x'] == s[0]['body_x'] and v['body_z'] == s[0]['body_z'] for v in s)
            assert s[-1]['detached']
            assert min(math.hypot(v['camera_x']-a['x'], v['camera_z']-a['z']) for v in s for a in v['actors'] if a['id']==j['watched_actor']) < 2
        else:
            assert any(v['recall'] for v in s) and any(v['cue_visible'] for v in s)
            assert not s[-1]['cue_visible'] and not s[-1]['recall']
            assert s[-1]['detached'] == (label == 'cancel')
            assert math.hypot(s[-1]['body_x']-s[0]['body_x'], s[-1]['body_z']-s[0]['body_z']) > 20
            assert s[-1]['thirst'] > s[0]['thirst']
            if label == 'cancel': assert all(not v['recall'] for v in s if v['physical_seconds'] > 1)
        metrics[label].update(recall_requests=s[-1]['recall_requests'], final_detached=s[-1]['detached'], captures=len(j['captures']))
    if label == 'nest':
        j = read(p / 'nesting.json'); s = j['samples']
        placed = [e for e in j['events'] if e['placed']]
        assert len(placed) == 1 and placed[0]['dry'] and placed[0]['clear'] and placed[0]['supported']
        assert j['eggs'] == 3 and not j['hatching_implemented'] and j['offspring_admitted'] == 0
        assert s[-1]['progress'] == 1 and s[-1]['incubated']
        laying_time = placed[0]['calendar_seconds']
        for v in s:
            if v['nest_objects']:
                assert abs(v['progress'] - min(1, max(0, v['calendar_seconds']-laying_time)/900)) < 1e-8
                assert v['nest_x'] == placed[0]['x'] and v['nest_z'] == placed[0]['z']
        assert any(v['direct'] and v['speed'] < 0 for v in s)
        assert len(j['captures']) >= 2 and all(c['saved'] and (p/c['filename']).is_file() for c in j['captures'])
        metrics[label].update(eggs=3, progress=1, offspring=0, captures=len(j['captures']))
    if label == 'normal':
        assert r['creature_loaded'] and r['animated_channel_pose_changed']
        assert not read(p/'nesting.json')['events']
        baseline = read(root/'var/reports/runtime/20261022-192749-99c9e857/world-population.json')
        assert w['actors_initial'] == baseline['actors_initial']
        assert w['resource_ledger_initial'] == baseline['resource_ledger_initial']

for path, expected in read(task/'preservation-before.json').items():
    if re.fullmatch(r'\.clanker/locks/slot-[123]/(owner_pid|started|task)', path):
        continue  # This pass acquired/released these slots through clinker-slot.sh.
    assert digest(root/path) == expected, ('preservation', path)
for name in ('connected-creature', 'connected-plant', 'connected-woodland'):
    pin = read(root/f'stalthos-runtime/config/{name}.json')
    entries = ([(root/pin['asset_root']/pin[k], pin[k+'_sha256']) for k in ('creature','metadata')]
               if name == 'connected-creature' else [(root/pin['path'], pin['sha256'])]
               if name == 'connected-plant' else [(root/pin[k]['path'], pin[k]['sha256']) for k in ('adult','sapling')])
    for path, expected in entries: assert digest(path) == expected, ('pinned asset', str(path))
metrics['executable_sha256'] = executable
metrics['preserved_untracked_files'] = sum(not p.startswith('.clanker/locks/') for p in read(task/'preservation-before.json'))
metrics['coordination_lock_lifecycle'] = 'Three task-owned slots acquired and released through the protocol helper; excluded from unrelated-file preservation.'
metrics['limits'] = 'Synthetic native fixtures; proxy encounter/nest proof, normal animated launch. No human input, hatching, connected save, complete game or FPS acceptance. New-life nest retention is source/domain proof only.'
(task/'verification.json').write_text(json.dumps(metrics, indent=2))
print('CONTINUATION_NATIVE_VERIFIED body cue/recall/cancellation/camera boundary; visible clutch/calendar progress; normal/original/preservation')
