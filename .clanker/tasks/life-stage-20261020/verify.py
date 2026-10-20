import json,pathlib,re,hashlib,math,statistics
root=pathlib.Path('.');task=root/'.clanker/tasks/life-stage-20261020'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def evidence(log):return pathlib.Path(re.search(r'evidence ([^\r\n]+)',(task/log).read_text(encoding='utf-8-sig'))[1])
folders={k:evidence(log) for k,log in {'proxy':'native-proxy.log','animal':'native-animal.log','normal':'native-normal.log','original':'original-mode.log'}.items()};metrics={};exe=read(task/'build-receipt.json')['executable_sha256']
for key,p in folders.items():
 r=read(p/'runtime-receipt.json');assert r['ok'] and read(p/'launch.json')['executable_sha256']==exe
 if key=='original':assert not (p/'life-stage.json').exists();assert r['terrain_support_longitudinal_halfspan_m']==.5;continue
 assert r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 j=read(p/'life-stage.json');s=j['samples'];assert s
 assert all(abs(v['age']-j['initial_age']-v['calendar_seconds'])<1e-7 and v['physical_seconds']==v['calendar_seconds'] for v in s)
 assert all(v['body_objects']>0 and all(math.isfinite(v[k]) and v[k]>0 for k in ['bounds_x','bounds_y','bounds_z']) for v in s)
 assert not j['ordinary_start_or_saved_age_modified'] and not j['clock_scale_modified']
 metrics[key]={'path':str(p),'initial_age':j['initial_age'],'milestones':j['milestones'],'support_failures':0,'support_fallbacks':0,'samples':len(s)}
 if key=='normal':
  assert not j['synthetic_start_age_fatigue_input_and_noon'] and j['initial_age']==0 and not j['milestones']
  assert all(v['bulk']==1 and v['stage']=='hatchling' for v in s);continue
 assert j['synthetic_start_age_fatigue_input_and_noon'] and j['initial_age']==2994
 assert len(j['milestones'])==1 and j['milestones'][0]['stage']=='adult' and 3000<=j['milestones'][0]['age']<3000.1
 assert 6<=j['milestones'][0]['physical_seconds']<6.1
 assert all(v['bulk']==(1 if v['stage']=='juvenile' else 1.25) for v in s)
 assert all(v['stage']==('juvenile' if v['age']<3000 else 'adult') for v in s)
 assert len(j['captures'])==4 and all(v['saved'] for v in j['captures'])
 assert j['captures'][0]['stage']=='juvenile' and j['captures'][1]['stage']=='adult'
 assert all(v['body_x']==24 and v['body_z']==24 for v in j['captures'][:2])
 before=[v for v in s if 1<v['physical_seconds']<4];after=[v for v in s if 6.5<v['physical_seconds']<8.5]
 ratios={axis:statistics.mean(v[axis] for v in after)/statistics.mean(v[axis] for v in before) for axis in ['bounds_x','bounds_z']}
 assert before and after and all(v['resting'] for v in before+after)
 for v in s:
  if v['physical_seconds']<8.8:assert abs(v['fatigue']-(.8-.05*v['physical_seconds']))<1e-6
 assert all(abs(v['thirst']-.0002*v['calendar_seconds'])<1e-7 for v in s)
 active=[v for v in s if v['direct']];assert active and all(not v['resting'] and v['stage']=='adult' for v in active)
 displacement=abs(s[-1]['body_x']-s[0]['body_x']);expected=4 if key=='proxy' else .32256
 assert abs(displacement-expected)<.04
 assert all(abs(v['scale_y']-(.4 if key=='proxy' else 1))<1e-6 for v in s)
 assert r['terrain_support_longitudinal_halfspan_m']==.625 and r['terrain_support_lateral_halfspan_m']==.3125
 if key=='proxy':assert 1.23<ratios['bounds_x']<1.27 and 1.22<ratios['bounds_z']<1.28
 else:assert ratios['bounds_x']>1.05 and ratios['bounds_z']>1.01
 metrics[key].update(stationary_renderer_bounds_ratio=ratios,applied_horizontal_bulk_after=1.25,post_stage_direct_displacement_m=displacement,rest_recovery_per_physical_second=.05,thirst_per_calendar_second=.0002)
normal=read(folders['normal']/'world-population.json');base=read(root/'var/reports/runtime/20261020-104737-302580ed/world-population.json')
assert normal['actors_initial']==base['actors_initial'] and len(normal['actors_initial'])==12
assert normal['resource_ledger_initial']==base['resource_ledger_initial'] and normal['resource_sessions_initial']==base['resource_sessions_initial']
for key in ['proxy','animal']:
 w=read(folders[key]/'world-population.json');assert w['actors_initial']==base['actors_initial'] and w['resource_ledger_initial']==base['resource_ledger_initial'] and w['resource_sessions_initial']==base['resource_sessions_initial']
prior=read(task/'preservation-before.json')
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 f=root/path;assert f.is_file() and hashlib.sha256(f.read_bytes()).hexdigest()==digest,path
pins=read(root/'stalthos-runtime/config/connected-woodland.json')
for stage in ['adult','sapling']:assert hashlib.sha256((root/pins[stage]['path']).read_bytes()).hexdigest()==pins[stage]['sha256']
pin=read(root/'stalthos-runtime/config/connected-creature.json')
for file,digest in [('creature','creature_sha256'),('metadata','metadata_sha256')]:assert hashlib.sha256((root/pin['asset_root']/pin[file]).read_bytes()).hexdigest()==pin[digest]
pin=read(root/'stalthos-runtime/config/connected-plant.json');assert hashlib.sha256((root/pin['path']).read_bytes()).hexdigest()==pin['sha256']
metrics['preserved_original_untracked_files']=len(prior)-1
metrics['baseline']='exact original8/all12 actors,11 resources and existing sessions in normal and both growth fixtures'
metrics['review']='root-only source/tests/native visual review; both preserved reviewers usage-limited'
metrics['scope']='one actual juvenile-to-adult calendar transition, horizontal bulk stand-in; conservative animated bounds vary with pose. No stage anatomy/gait/sole/family/save/performance acceptance'
metrics['rest_after_direct_release']='existing rest hysteresis preserved; remaining fatigue below entry threshold, so automatic resume is not claimed'
(task/'verification.json').write_text(json.dumps(metrics,indent=2));print('LIFE_STAGE_NATIVE_VERIFIED real age3000 crossing; proxy/animal bounds, physical recovery and direct movement; ordinary baseline and preservation passed')
