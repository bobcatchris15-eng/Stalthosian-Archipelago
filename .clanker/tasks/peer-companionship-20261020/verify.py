import json,pathlib,re,hashlib,math
root=pathlib.Path('.');task=root/'.clanker/tasks/peer-companionship-20261020'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def evidence(log):return pathlib.Path(re.search(r'evidence ([^\r\n]+)',(task/log).read_text(encoding='utf-8-sig'))[1])
folders={k:evidence(v) for k,v in {'peer':'final-peer.log','camera':'final-native-camera.log','animal':'final-animal.log','normal':'final-normal.log','original':'original-mode.log'}.items()}
metrics={};executable=read(task/'build-receipt.json')['executable_sha256']
for key,p in folders.items():
 r=read(p/'runtime-receipt.json');assert r['ok']
 assert read(p/'launch.json')['executable_sha256']==executable
 if key=='original':assert not (p/'peer-companionship.json').exists();continue
 assert r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 j=read(p/'peer-companionship.json');s=j['samples'];metrics[key]={'path':str(p),'peer':j['peer'],'events':j['events'],'samples':len(s),'support_failures':0,'support_fallbacks':0}
 if key=='peer':
  peer=j['peer'];assert peer and [v['present'] for v in j['events']]==[True,False,True] and all(v['peer']==peer for v in j['events'])
  assert len(j['captures'])==4 and all(v['saved'] for v in j['captures'])
  assert any(v['direct'] for v in s) and any(not v['direct'] and v['physical_seconds']>39 for v in s)
  def actor(v):return next(a for a in v['actors'] if a['id']==peer)
  toward=0
  for a,b in zip(s,s[1:]):
   if a['motion_peer']!=peer or b['motion_peer']!=peer:continue
   pa,pb=actor(a),actor(b);dx,dz=pb['x']-pa['x'],pb['z']-pa['z']
   if math.hypot(dx,dz)>1e-5 and dx*(a['body_x']-pa['x'])+dz*(a['body_z']-pa['z'])>0:toward+=1
  assert toward>3
  metrics[key]['actor_distance_m']=actor(s[-1])['distance']-actor(s[0])['distance'];metrics[key]['observed_toward_body_segments']=toward
  assert metrics[key]['actor_distance_m']>10
  assert max(v['body_x'] for v in s)-min(v['body_x'] for v in s)>27
 if key=='camera':
  assert j['peer']==0 and not j['events'] and all(v['motion_peer']==0 and not v['present'] for v in s)
  assert all(v['body_x']==140 and v['body_z']==18 for v in s)
  late=[v for v in s if v['physical_seconds']>5]
  span=max(v['camera_x'] for v in late)-min(v['camera_x'] for v in late);near=min(math.hypot(v['camera_x']-116,v['camera_z']-18) for v in s)
  assert span>3.5 and near<1
  metrics[key].update(camera_focus_span_m=span,min_focus_distance_to_actor_m=near,body_distance_to_actor_m=24,configured_camera_distance_m=5)
 if key=='animal':assert j['peer']==metrics['peer']['peer'] and j['events'][0]['present'] and all(v['saved'] for v in j['captures'])
 if key=='normal':assert not j['synthetic_start_satisfied_npc_input_and_noon']
normal=read(folders['normal']/'world-population.json');baseline=read(root/'var/reports/runtime/20261020-093841-18911a09/world-population.json')
assert normal['actors_initial']==baseline['actors_initial'] and len(normal['actors_initial'])==12
assert normal['resource_ledger_initial']==baseline['resource_ledger_initial'] and normal['resource_sessions_initial']==baseline['resource_sessions_initial']
peer=metrics['peer']['peer'];assert sum(a['id']==peer and a['species']=='Smalltrotter' for a in baseline['actors_initial'])==1
for key in ['peer','animal','camera']:
 w=read(folders[key]/'world-population.json');assert {a['id'] for a in w['actors_initial']}=={a['id'] for a in baseline['actors_initial']}
prior=read(task/'preservation-before.json')
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 p=root/path;assert p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()==digest,path
for stage in ['adult','sapling']:
 pin=read(root/'stalthos-runtime/config/connected-woodland.json')[stage];assert hashlib.sha256((root/pin['path']).read_bytes()).hexdigest()==pin['sha256']
pin=read(root/'stalthos-runtime/config/connected-creature.json')
for file,digest in [('creature','creature_sha256'),('metadata','metadata_sha256')]:assert hashlib.sha256((root/pin['asset_root']/pin[file]).read_bytes()).hexdigest()==pin[digest]
pin=read(root/'stalthos-runtime/config/connected-plant.json');assert hashlib.sha256((root/pin['path']).read_bytes()).hexdigest()==pin['sha256']
metrics['preserved_original_untracked_files']=len(prior)-1
metrics['baseline']='exact original8/all12 actors and11-resource/session initial state; same existing peer ID, no new population identity'
metrics['review']='root-only source/tests/native review; both preserved reviewers usage-limited'
metrics['scope']='session-only affiliation, graph wait/short approach; labelled synthetic startup/satisfied NPC/input/noon. Camera/body separation verified; no full family/save/human input/art/FPS claim'
(task/'verification.json').write_text(json.dumps(metrics,indent=2));print('PEER_NATIVE_VERIFIED',peer,'actor moved',metrics['peer']['actor_distance_m'],'m;',metrics['peer']['observed_toward_body_segments'],'toward-body segments; separation/return and camera boundary passed')
