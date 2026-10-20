import pathlib,json,re,hashlib,math
root=pathlib.Path('.');task=root/'.clanker/tasks/npc-peer-20261020'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def folder(label):return pathlib.Path(re.search(r'evidence ([^\r\n]+)',(task/f'native-{label}-published.log').read_text(encoding='utf-8-sig'))[1])
exe=read(task/'build-receipt.json')['executable_sha256'];metrics={};runs={};leader=3572998927451126612;follower=12326054023193623710
base=read(root/'var/reports/runtime/20261020-113545-5ed05743/world-population.json')
for label in ['pair','camera','need','normal','original']:
 p=folder(label);r=read(p/'runtime-receipt.json');assert r['ok'] and r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 assert read(p/'launch.json')['executable_sha256']==exe
 w=read(p/'world-population.json');runs[label]=(p,w)
 assert w['registry_owns_population'] and w['registry_owns_shared_ledger'] and not w['camera_drives_simulation']
 assert w['resource_sessions_initial']==w['resource_sessions_final'] and not w['resource_history_rejections']
 assert [a['id'] for a in w['actors_initial']]==[a['id'] for a in w['actors_final']]
 for resource in w['resource_ledger_final']['resources']:
  assert 0<=resource['consumed_quantity']<=resource['initial_quantity']
  total=sum(s['amount'] for s in w['resource_ledger_final']['sessions'] if s['resource_id']==resource['id'])
  assert abs(total-resource['consumed_quantity'])<1e-8
 metrics[label]={'path':str(p),'support_failures':0,'support_fallbacks':0,'actor_count':len(w['actors_initial']),'retained_sessions':len(w['resource_sessions_final']),'history_keys':w['resource_history_used']}
 if label=='original':
  old=read(root/'var/reports/runtime/20261020-155440-55a39585/world-population.json')
  assert not (p/'npc-peer-association.json').exists()
  assert w['actors_initial']==old['actors_initial'] and w['resource_ledger_initial']==old['resource_ledger_initial'];continue
 j=read(p/'npc-peer-association.json')
 if label!='normal':assert read(p/'day-night.json')['synthetic_renderer_phase'] is True
 assert w['resource_ledger_initial']==base['resource_ledger_initial'] and w['resource_sessions_initial']==base['resource_sessions_initial']
 if label=='normal':assert w['actors_initial']==base['actors_initial'] and j['probe_mode']==0;continue
 for a,b in zip(w['actors_initial'],base['actors_initial']):
  if a['id'] not in [leader,follower]:assert a==b
 assert all(s['body_x']==124 and s['body_z']==36 and s['hero_peer']==0 for s in j['samples'])
 formed=[s for s in j['samples'] if s['follower']];assert formed and all(s['leader']==leader and s['follower']==follower and s['present'] for s in formed)
 last=formed[-1];actors={a['id']:a for a in last['actors']};gap=math.hypot(actors[leader]['x']-actors[follower]['x'],actors[leader]['z']-actors[follower]['z'])
 assert abs(gap-3)<1e-5 and last['waiting_follower']==follower and actors[leader]['distance']==0 and .99<actors[follower]['distance']<1.01
 assert all(c['saved'] and (p/c['filename']).is_file() for c in j['captures']) and len(j['captures'])==3
 metrics[label].update(leader=leader,follower=follower,final_gap_m=gap,follower_travel_m=actors[follower]['distance'],formation_physical_seconds=j['events'][0]['physical_seconds'],waiting_follower=last['waiting_follower'])
 if label=='need':
  binding=dict(w['resource_sessions_final'])[leader];amount=sum(s['amount'] for s in w['resource_ledger_final']['sessions'] if s['session_id']==binding and s['resource_id']==209)
  initial=next(a for a in w['snapshot_initial']['actors'] if a['id']==leader);final=next(a for a in w['snapshot_final']['actors'] if a['id']==leader)
  expected=initial['thirst']+(final['age']-initial['age'])*.0002-amount*.2
  assert amount>0 and abs(final['thirst']-expected)<1e-9
  drinks=[s for s in j['samples'] if any(a['id']==leader and a['activity']==3 for a in s['actors'])];assert drinks
  metrics[label].update(accepted_shared_water_amount=amount,exact_relief=amount*.2,native_drink_samples=len(drinks),final_thirst=final['thirst'])
a=read(runs['pair'][0]/'npc-peer-association.json');b=read(runs['camera'][0]/'npc-peer-association.json')
assert runs['pair'][1]['snapshot_initial']==runs['camera'][1]['snapshot_initial']
assert runs['pair'][1]['resource_ledger_initial']==runs['camera'][1]['resource_ledger_initial']
def position_at(j,t):
 samples=[s for s in j['samples'] if s['follower']]
 for x,y in zip(samples,samples[1:]):
  if x['physical_seconds']<=t<=y['physical_seconds']:
   f=(t-x['physical_seconds'])/(y['physical_seconds']-x['physical_seconds']);p=next(a for a in x['actors'] if a['id']==follower);q=next(a for a in y['actors'] if a['id']==follower);return tuple(p[k]+f*(q[k]-p[k]) for k in ['x','z'])
 raise AssertionError('missing comparison time')
errors=[]
for t in [2,2.5,3,4,8,14,18]:
 p=position_at(a,t);q=position_at(b,t);errors.append(math.hypot(p[0]-q[0],p[1]-q[1]))
assert max(errors)<.04
cam=read(runs['camera'][0]/'hero-autonomy.json')['samples'];span=math.hypot(max(s['camera_target_x'] for s in cam)-min(s['camera_target_x'] for s in cam),max(s['camera_target_z'] for s in cam)-min(s['camera_target_z'] for s in cam));assert span>15
prior=read(task/'preservation-before.json')
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 assert (root/path).is_file() and hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
for name in ['connected-creature','connected-plant','connected-woodland']:
 pin=read(root/f'stalthos-runtime/config/{name}.json');entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ['creature','metadata']] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ['adult','sapling']])
 for path,digest in entries:assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
metrics['camera_comparison']={'identical_initial_actor_body_ledger_sessions':True,'max_interpolated_position_error_m':max(errors),'camera_span_m':span,'hero_peer_always_zero':True}
metrics['preserved_original_untracked']=len(prior)-('.clanker/graph/built-at-rev' in prior)
metrics['review']='root-only; both preserved independent workers usage-limited; zero dispatches'
metrics['limits']='native short approach/wait and eligible needs/contact; wider departure/reunion validated in domain only, not a native full route; graph edge completion, horizontal sensing, idle poses and original creature/ground limits remain'
(task/'verification.json').write_text(json.dumps(metrics,indent=2));print('NPC_PEER_NATIVE_VERIFIED actual approach/separate idle bodies, same-start moving-camera invariance, exact need relief, IDs/sessions/finite history and preservation')
