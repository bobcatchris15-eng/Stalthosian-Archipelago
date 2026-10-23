from pathlib import Path
import hashlib, json, math

root = Path(__file__).resolve().parents[3]
task = Path(__file__).parent
read = lambda p: json.loads(p.read_text(encoding='utf-8-sig'))
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
executable = sha(root/'var/build/stalthos-runtime/Release/Stalthos.exe')
metrics = {}
for label, path in read(task/'native-runs.json').items():
    p = Path(path); r = read(p/'runtime-receipt.json'); w = read(p/'world-population.json')
    assert r['ok'] and r['support_failure_count'] == 0
    assert r['rendered_ground_support_fallbacks'] == 0
    assert read(p/'launch.json')['executable_sha256'] == executable
    initial = {a['id']: a for a in w['snapshot_initial']['actors']}
    final = {a['id']: a for a in w['snapshot_final']['actors']}
    for identity, actor in initial.items():
        assert identity in final and abs(final[identity]['age']-actor['age']-r['calendar_seconds']) < 1e-6
    ledger = w['resource_ledger_final']
    for resource in ledger['resources']:
        total = sum(s['amount'] for s in ledger['sessions'] if s['resource_id'] == resource['id'])
        assert abs(total-resource['consumed_quantity']) < 1e-8
    metrics[label] = {'path':path,'physical_seconds':r['physical_seconds'],'support_failures':0,'support_fallbacks':0,'actors':len(final),'sessions':len(w['resource_sessions_final'])}
    if label in ('hatch','camera'):
        j = read(p/'nesting.json'); s = j['samples']
        births = [e for e in j['hatch_events'] if e['event']=='admission_attempt' and e['result']==0]
        assert len(births)==1 and j['offspring_admitted']==1 and j['hatching_implemented']
        birth=births[0]; child=birth['child']; binding=dict(w['resource_sessions_final'])
        assert len(initial)==12 and len(final)==13 and set(final)-set(initial)=={child}
        assert birth['old_actors_unchanged'] and birth['ledger_unchanged']
        assert birth['old_session_count']==12 and birth['session_count']==13
        assert binding[child]==birth['session'] and len(set(binding.values()))==13
        assert {k:v for k,v in binding.items() if k!=child}==dict(w['resource_sessions_initial'])
        assert abs(final[child]['age']-(r['calendar_seconds']-birth['birth_calendar']))<1e-6
        assert j['eggs']==2 and j['closed_shells']==1
        assert len(j['captures'])==5 and all(c['saved'] and (p/c['filename']).is_file() for c in j['captures'])
        alive_samples=[v for v in s if 'child' in v]
        assert alive_samples and all(v['offspring']==child and v['actor_count']==13 and v['replica_count']==13 for v in alive_samples)
        assert all(v['child_session']==birth['session'] and v['hero_session']!=birth['session'] for v in alive_samples)
        assert all(abs(v['child']['age']-(v['calendar_seconds']-birth['birth_calendar']))<1e-6 for v in alive_samples)
        error=max(v['child_rendered_ground_error'] for v in alive_samples)
        assert error<.01 and not any(v.get('child_ground_missing',False) for v in alive_samples)
        assert final[child]['distance_m']>.1 and alive_samples[-1]['child']['thirst']>alive_samples[0]['child']['thirst']
        xs=[v['child']['x'] for v in alive_samples];zs=[v['child']['z'] for v in alive_samples]
        metrics[label].update(child=child,child_session=birth['session'],birth_physical_seconds=birth['physical_seconds'],child_age=final[child]['age'],child_cumulative_distance_m=final[child]['distance_m'],sampled_child_position_span_m=math.hypot(max(xs)-min(xs),max(zs)-min(zs)),max_child_ground_error_m=error,captures=5)
        if label=='hatch':
            next_life=read(p/'death-next-life.json')['events']
            assert len(next_life)==1 and next_life[0]['hero_id']==3 and child==2
            assert next_life[0]['npc_snapshot_unchanged_at_handoff'] and next_life[0]['ledger_unchanged_at_handoff']
            assert s[-1]['hero']==3 and s[-1]['offspring']==2
            assert any(v['direct'] and v['speed']<0 for v in s) and any(v['speed']>0 for v in s)
            moving=[v for v in s if 12<v['physical_seconds']<24]
            span=max(v['body_z'] for v in moving)-min(v['body_z'] for v in moving)
            assert span>.6 and abs(s[-1]['body_z']-s[0]['body_z'])<.01
            metrics[label].update(successor=3,parent_local_departure_m=span,retained_through_next_life=True)
        else:
            span=math.hypot(max(v['camera_x'] for v in s)-min(v['camera_x'] for v in s),max(v['camera_z'] for v in s)-min(v['camera_z'] for v in s))
            assert span>20 and not any(v['direct'] for v in s)
            assert w['physical_active_initial'] and w['physical_active_final'] and not w['activation_transitions']
            metrics[label].update(camera_span_m=span,scope='After-birth retained-young observation; normal body autonomy remains active. Inactive birth refusal is source/domain evidence.')
    else:
        assert len(initial)==len(final)==(12 if label=='normal' else 8)
        assert w['resource_sessions_initial']==w['resource_sessions_final']
        baseline=read(root/('var/reports/runtime/20261022-194453-ee2f4bae/world-population.json' if label=='normal' else 'var/reports/runtime/20261020-155440-55a39585/world-population.json'))
        assert w['actors_initial']==baseline['actors_initial'] and w['resource_ledger_initial']==baseline['resource_ledger_initial']
        assert r['creature_loaded'] and r['animated_channel_pose_changed']
        if label=='normal': assert not read(p/'nesting.json')['offspring_admitted']
        else: assert not (p/'nesting.json').exists()

for path, expected in read(task/'preservation-before.json').items(): assert sha(root/path)==expected,path
for name in ('connected-creature','connected-plant','connected-woodland'):
    pin=read(root/f'stalthos-runtime/config/{name}.json')
    entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ('creature','metadata')] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ('adult','sapling')])
    for path, expected in entries: assert sha(path)==expected,str(path)
metrics['executable_sha256']=executable
metrics['preserved_untracked_files']=len(read(task/'preservation-before.json'))
metrics['limits']='Synthetic adult/time/input/death fixtures; local parent movement only, not regional traversal. Camera moves after birth and body autonomy may act; no zero-attention claim. Inactive admission, full-capacity and dormant retention have independent source/domain proof. No hatchling resource-contact, final anatomy/gait/growth art, mating, connected-save, full-game or FPS acceptance.'
(task/'verification.json').write_text(json.dumps(metrics,indent=2))
print('HATCHLING_NATIVE_VERIFIED one real local child, unique ownership, needs/motion/ground, next-life retention, camera observation and unchanged ordinary baselines')
