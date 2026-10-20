import json, pathlib, hashlib
root=pathlib.Path('.');task=root/'.clanker/tasks/day-night-20261020';reports=root/'var/reports/runtime'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def report(name,file):return read(reports/name/file)
preview='20261020-083639-5a16f689';controls='20261020-083726-63cb457b';normal='20261020-083805-d367069b';original='20261020-083837-c9302bdd'
metrics={}
for name in [preview,controls,normal]:
 j=report(name,'day-night.json');s=j['samples'];r=report(name,'runtime-receipt.json')
 assert r['ok'] and r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 assert all(abs(v['physical_seconds']-v['calendar_seconds'])<1e-9 for v in s)
 assert max(v['direction_error'] for v in s)<1e-5 and max(v['sky_light_direction_error'] for v in s)<1e-5
 assert not j['renderer_modifies_clock_or_needs']
 metrics[name]={'samples':len(s),'physical_seconds':s[-1]['physical_seconds'],'max_direction_error':max(v['direction_error'] for v in s),'max_sky_light_error':max(v['sky_light_direction_error'] for v in s),'support_failures':0,'support_fallbacks':0}
p=report(preview,'day-night.json');assert p['synthetic_renderer_phase'] and not p['synthetic_initial_fatigue_and_body_input']
assert [v['rendered_fraction'] for v in p['captures']]==[.28,.5,.72,0]
assert all(v['saved'] for v in p['captures']) and all(v['body_x']==24 and v['body_z']==24 for v in p['samples'])
c=report(controls,'day-night.json');s=c['samples'];assert c['synthetic_initial_fatigue_and_body_input']
assert all(v['rendered_fraction']==0 for v in s)
assert any(v['direct'] for v in s) and all(not v['resting'] for v in s if v['direct'])
assert s[0]['resting'] and any(v['resting'] and v['physical_seconds']>4 for v in s)
move=abs(s[-1]['body_x']-s[0]['body_x']);assert 3.8<move<4.2
rates=[(b['fatigue']-a['fatigue'])/(b['physical_seconds']-a['physical_seconds']) for a,b in zip(s,s[1:]) if a['resting'] and b['resting'] and a['physical_seconds']>5]
assert rates
r=[v for v in s if v['resting'] and v['physical_seconds']>5];restRate=(r[-1]['fatigue']-r[0]['fatigue'])/(r[-1]['physical_seconds']-r[0]['physical_seconds']);assert -.055<restRate<-.045
metrics['night_controls']={'body_displacement_m':move,'rest_recovery_rate_over_observed_rest_interval':restRate,'instantaneous_sample_rate_range':[min(rates),max(rates)],'sample_timing_note':'frame-order quantization affects adjacent samples; aggregate interval used','synthetic_body_input':True,'human_physical_input_tested':False}
n=report(normal,'day-night.json');assert not n['synthetic_renderer_phase'] and n['probe_mode']==0
assert all(abs(v['rendered_fraction']-v['calendar_fraction'])<1e-12 for v in n['samples'])
b=report('20261020-073231-f0c1fb9b','world-population.json');w=report(normal,'world-population.json')
assert w['actors_initial']==b['actors_initial'] and len(w['actors_initial'])==12
assert w['resource_ledger_initial']==b['resource_ledger_initial']
assert w['resource_sessions_initial']==b['resource_sessions_initial']
assert report(original,'runtime-receipt.json')['ok'] and not (reports/original/'day-night.json').exists()
prior=read(task/'preservation-before.json');changed=[]
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 f=root/path
 if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=digest:changed.append(path)
assert not changed,changed
metrics['preserved_original_untracked_files']=len(prior)-1
metrics['baseline']='original8/all12 actors and original11 resource registry/session initial state exactly preserved'
metrics['review']='root implementation and source/native review only; both preserved independent reviewers usage-limited'
metrics['visual_review']='four composed native phases inspected; final night readable; simple black sky and unshadowed blue fill remain; no final art/FPS/elapsed24h claim'
(task/'verification.json').write_text(json.dumps(metrics,indent=2))
print('DAY_NIGHT_NATIVE_VERIFIED',move,'m; phase/clock/light/rest/population/resource/preservation checks passed')
