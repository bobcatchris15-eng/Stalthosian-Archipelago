import pathlib,json,re,math,hashlib
root=pathlib.Path('.');task=root/'.clanker/tasks/exterior-shore-20261020'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def folder(log):return pathlib.Path(re.search(r'evidence ([^\r\n]+)',(task/log).read_text(encoding='utf-8-sig'))[1])
logs={'route':'native-route-final.log','entry':'native-entry-final.log','still':'native-still-bounded.log','overview':'native-camera-bounded.log','normal':'native-normal-final.log','original':'native-original-final.log'}
exe=read(task/'build-receipt.json')['executable_sha256'];assert exe!='44b1514b8b29a3fbe33b0ce1526f6861f3b7dfb66a3067f3db247a804ce18ba7';metrics={};runs={};base=read(root/'var/reports/runtime/20261020-113545-5ed05743/world-population.json')
for label,log in logs.items():
 p=folder(log);r=read(p/'runtime-receipt.json');w=read(p/'world-population.json');runs[label]=(p,r,w)
 assert r['ok'] and r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0 and read(p/'launch.json')['executable_sha256']==exe
 assert w['registry_owns_population'] and w['registry_owns_shared_ledger'] and not w['camera_drives_simulation']
 assert w['resource_sessions_initial']==w['resource_sessions_final'] and not w['resource_history_rejections']
 assert [a['id'] for a in w['actors_initial']]==[a['id'] for a in w['actors_final']]
 for resource in w['resource_ledger_final']['resources']:
  assert 0<=resource['consumed_quantity']<=resource['initial_quantity']
  total=sum(s['amount'] for s in w['resource_ledger_final']['sessions'] if s['resource_id']==resource['id']);assert abs(total-resource['consumed_quantity'])<1e-8
 initial={a['id']:a for a in w['snapshot_initial']['actors']}
 for a in w['snapshot_final']['actors']:assert abs(a['age']-initial[a['id']]['age']-r['calendar_seconds'])<1e-6
 metrics[label]={'path':str(p),'support_failures':0,'support_fallbacks':0,'physical_seconds':r['physical_seconds'],'actor_count':len(w['actors_final']),'retained_sessions':len(w['resource_sessions_final']),'history_keys':w['resource_history_used']}
 if label=='original':
  old=read(root/'var/reports/runtime/20261020-155440-55a39585/world-population.json');assert not (p/'exterior-shore.json').exists() and w['actors_initial']==old['actors_initial'] and w['resource_ledger_initial']==old['resource_ledger_initial'];continue
 assert w['actors_initial']==base['actors_initial'] and w['resource_ledger_initial']==base['resource_ledger_initial'] and w['resource_sessions_initial']==base['resource_sessions_initial']
 j=read(p/'exterior-shore.json');assert j['authored_cove_preview'] and j['rock_instances']==14 and j['sea_level']==0 and j['preserved_meadow_through_x']==200
 t=read(p/'populated-terrain.json');assert t['max_shared_height_error_m']==0 and t['regenerated_mesh_mismatches']==0 and t['residency_driver']=='body'
 if label=='normal':assert j['probe_mode']==0 and read(p/'hero-exploration.json')['enabled'];continue
 assert read(p/'day-night.json')['synthetic_renderer_phase'] and len(j['captures'])==(3 if label in ['still','overview'] else 4) and all(c['saved'] and (p/c['filename']).is_file() for c in j['captures'])
 s=j['trajectory'];assert s and all(v['z']==24 for v in s)
 if label in ['route','entry']:
  start=184 if label=='route' else 254;assert start<=s[0]['x']<start+.1 and 258<=s[-1]['x']<258.1 and s[-1]['ground_y']>.45
  assert all(abs(v['step_distance_m']-abs(v['integrator_speed_m_s']*v['dt']))<.000035 for v in s)
  grades=[abs(b['ground_y']-a['ground_y'])/(b['x']-a['x']) for a,b in zip(s,s[1:]) if b['x']-a['x']>.001];assert max(grades)<.4
  moving=[v for v in s if v['forward_intent']];assert moving and all(v['integrator_speed_m_s']>0 and v['ground_y']>0 for v in moving)
  assert t['body_tile_crossings']>=2 and not w['physical_active_final']
  metrics[label].update(distance_m=s[-1]['x']-start,max_sampled_grade=max(grades),final_ground_y=s[-1]['ground_y'],body_tile_crossings=t['body_tile_crossings'],speed_m_s=moving[0]['integrator_speed_m_s'])
 else:
  assert all(v['x']==184 and v['step_distance_m']==0 and not v['forward_intent'] for v in s)
  assert w['physical_active_initial'] and w['physical_active_final'] and w['activation_transitions']==[]
a=runs['still'];b=runs['overview'];assert a[2]['snapshot_initial']==b[2]['snapshot_initial'] and a[2]['resource_ledger_initial']==b[2]['resource_ledger_initial']
for key in ['created_tiles','resident_tiles','visited_body_tiles','body_tile_crossings']:assert read(a[0]/'populated-terrain.json')[key]==read(b[0]/'populated-terrain.json')[key]
for p,r,w in [a,b]:
 s=read(p/'hero-autonomy.json')['samples'];assert all(not x['body_attention'] and not x['auto_active'] and not x['auto_contact'] and x['target_x']==0 and x['target_z']==0 for x in s)
cam=[s for s in read(b[0]/'hero-autonomy.json')['samples'] if s['elapsed']>=2];assert all(217<=s['camera_target_x']<=231 for s in cam);span=math.hypot(max(v['camera_target_x'] for v in cam)-min(v['camera_target_x'] for v in cam),max(v['camera_target_z'] for v in cam)-min(v['camera_target_z'] for v in cam));assert span>11
final_a={x['id']:x for x in a[2]['actors_final']};final_b={x['id']:x for x in b[2]['actors_final']};error=max(math.hypot(final_a[i]['x']-final_b[i]['x'],final_a[i]['z']-final_b[i]['z']) for i in final_a);dt=abs(a[1]['physical_seconds']-b[1]['physical_seconds']);assert error<.7*dt+.05
prior=read(task/'preservation-before.json')
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 assert (root/path).is_file() and hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
for name in ['connected-creature','connected-plant','connected-woodland']:
 pin=read(root/f'stalthos-runtime/config/{name}.json');entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ['creature','metadata']] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ['adult','sapling']])
 for path,digest in entries:assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
metrics['camera_boundary']={'identical_initial_body_actors_ledger_sessions':True,'body_fixed_x':184,'camera_span_m':span,'activation_unchanged':True,'terrain_footprint_unchanged':True,'hero_attention_and_goals_empty':True,'final_actor_position_difference_m':error,'physical_duration_difference_s':dt}
metrics['preserved_original_untracked']=len(prior)-('.clanker/graph/built-at-rev' in prior);metrics['review']='root-only; both preserved reviewers usage-limited; zero dispatches';metrics['limits']='authored finite cove, flat water/visible sheet edge/coarse cliffs/material bands; local animated4m entry and proxy74m descent, not full animated watershed-to-coast route, global shoreline, foot-contact, hydrology/swimming or full-game/FPS acceptance'
(task/'verification.json').write_text(json.dumps(metrics,indent=2));print('EXTERIOR_SHORE_NATIVE_VERIFIED supported74m proxy descent and4m animated entry; camera/body/activation/knowledge boundary, normal/original baseline and preservation')
