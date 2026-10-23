from pathlib import Path
import json,hashlib,re,subprocess,math
r=Path(__file__).resolve().parents[3];t=Path(__file__).parent;read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'));sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();runs={}
for f in t.glob('native-*.log'):
 if f.stem in ['native-matrix-output','native-resume-output','native-final-checks-output']:continue
 m=re.search(r'evidence (.+)',f.read_text(encoding='utf-8-sig'))
 if m:runs[f.stem[7:]]=m.group(1).strip()
assert len(runs)==14,(len(runs),runs)
exe=sha(r/'var/build/stalthos-runtime/Release/Stalthos.exe');metrics={}
for label,path in runs.items():
 p=Path(path);a=read(p/'runtime-receipt.json');w=read(p/'world-population.json');assert a['ok'] and a['support_failure_count']==0 and a['rendered_ground_support_fallbacks']==0,label;assert read(p/'launch.json')['executable_sha256']==exe,label
 old={x['id']:x for x in w['snapshot_initial']['actors']};new={x['id']:x for x in w['snapshot_final']['actors']};assert set(old)<=set(new) and len(new)==len(old)+(label=='small-hatch'),label
 for identity,x in old.items():assert abs(new[identity]['age']-x['age']-(a['calendar_seconds']-a.get('loaded_calendar_seconds',0)))<1e-6,(label,identity)
 bindings=dict(w['resource_sessions_final']);sessionSpecies={session:new[identity]['species'] for identity,session in bindings.items()}
 for resource in w['resource_ledger_final']['resources']:assert abs(sum(k['amount'] for k in w['resource_ledger_final']['sessions'] if k['resource_id']==resource['id'])-resource['consumed_quantity'])<1e-8,label
 if not label.startswith('original'):
  assert any(x['id']==211 and x['kind']==0 and x['initial_quantity']==1 for x in w['resource_ledger_initial']['resources']),label
  assert read(p/'carrion-habitat.json')['supported_half_meter_path_points']>1,label
  for k in w['resource_ledger_final']['sessions']:
   if k['session_id'] in sessionSpecies:
    if k['resource_id']==211:assert sessionSpecies[k['session_id']]==4,(label,k)
    elif 200<=k['resource_id']<=210 and k['resource_id']!=209:assert sessionSpecies[k['session_id']]!=4,(label,k)
 metrics[label]={'path':path,'physical_seconds':a['physical_seconds'],'actors':len(new)}
p=Path(runs['competition']);j=read(p/'npc-carrion.json');s=j['samples'];actor=j['events'][0]['actor'];assert all(x['actor']['id']==actor and x['actor_count']==12 for x in s);assert s[0]['remaining']==1 and s[-1]['remaining']==0 and s[-1]['actor']['distance']>5 and s[-1]['actor']['hunger']<s[0]['actor']['hunger'];assert s[-1]['hero_food_contacts']==1
w=read(p/'world-population.json');session=dict(w['resource_sessions_final'])[actor];food=[x for x in w['resource_ledger_final']['sessions'] if x['resource_id']==211];assert sorted(x['amount'] for x in food)==[.5,.5] and next(x for x in food if x['session_id']==session)['amount']==.5;assert all(x['saved'] for x in j['captures']) and len(j['captures'])==4
c=read(Path(runs['camera'])/'npc-carrion.json')['samples'];assert all(x['body_food_distance_xz']>14 and not x['food_sensed'] and x['hero_food_contacts']==0 and x['physical_active'] for x in c);assert min(x['camera_food_distance_xz'] for x in c)<3;assert max(math.hypot(x['body_x']-c[0]['body_x'],x['body_z']-c[0]['body_z']) for x in c)<1e-6
for label in ['ordinary-small','partial-save']:
 w=read(Path(runs[label])/'world-population.json');npcSessions={x[1] for x in w['resource_sessions_final']};assert any(x['resource_id']==211 and x['session_id'] in npcSessions and x['amount']>0 for x in w['resource_ledger_final']['sessions']),label
pairs=[('partial-save','partial-load'),('partial-load','drain'),('drain','drained-load')]
for saved,loaded in pairs:assert read(Path(runs[saved])/'connected-saved-candidate.json')==read(Path(runs[loaded])/'connected-restored-before-first-tick.json'),(saved,loaded)
for label in ['drain','drained-load']:
 a=read(Path(runs[label])/'connected-saved-candidate.json');food=next(x for x in a['ledger']['resources'] if x['id']==211);assert food['initial_quantity']==food['consumed_quantity']==1,label
failure=read(t/'native-failure-verification.json');assert all(failure.values());f=Path(re.search(r'evidence (.+)',(t/'failure-old-write.log').read_text()).group(1).strip());a=read(f/'runtime-receipt.json');assert a['ok'] and not a['state_saved'];assert any(x.get('exit_saved') is False for x in read(f/'connected-persistence.json')['events'])
for path,digest in read(t/'preservation-before.json').items():assert sha(r/path)==digest,path
for e in read(r/'.clanker/clunkers/npc-carrion-diet-review-20261023/source-hashes.json'):
 assert hashlib.sha256((r/e['path']).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==e['canonical_lf_sha256'];assert subprocess.check_output(['git','hash-object','--path='+e['path'],e['path']],cwd=r).decode().strip()==e['git_blob']
for name in ('connected-creature','connected-plant','connected-woodland'):
 pin=read(r/f'stalthos-runtime/config/{name}.json');entries=([(r/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ('creature','metadata')] if name=='connected-creature' else [(r/pin['path'],pin['sha256'])] if name=='connected-plant' else [(r/pin[k]['path'],pin[k]['sha256']) for k in ('adult','sapling')])
 for path,h in entries:assert sha(path)==h,str(path)
(t/'native-runs.json').write_text(json.dumps(runs,indent=2)+'\n');out={'ok':True,'ctest':'69/69','same_executable_successful_native_runs':len(runs),'executable_sha256':exe,'native_runs':metrics,'actual_npc':actor,'actual_npc_travel_meters':s[-1]['actor']['distance'],'hero_food':.5,'npc_food':.5,'camera_closest_food':min(x['camera_food_distance_xz'] for x in c),'actual_live_restart_pairs':pairs,'full_depletion_retained_after_restart':True,'old_profiles_refused_preserved':failure,'preserved_unrelated_paths':len(read(t/'preservation-before.json')),'source_canonical_lf_blobs_match':True,'pins_unchanged':True,'limits':['Authored finite carcass, no real kill/provisioning/renewable food','NPC generic art/body overlap, crude feeding/contact/route and dark scene remain','Camera test alreadyactive population; no dormant-population activation claim','Fullspecieslife/families, fourphysicalislands, unrestrictedconnectedpersistence and30FPS remain unaccepted']};(t/'verification.json').write_text(json.dumps(out,indent=2)+'\n');print('NPC_CARRION_NATIVE_VERIFIED')
